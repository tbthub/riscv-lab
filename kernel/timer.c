#include "core/timer.h"
#include "mm/slab.h"
#include "std/stdio.h"
#include "core/proc.h"
#include "lib/fifo.h"
#include "core/work.h"
#include "lib/sleeplock.h"
#include "mm/kmalloc.h"

static ticks_t sys_ticks;

// 我们对每个CPU都分配一个定时器,避免多 CPU 造成的竞争，
// 同时，由于时钟中断后检查到期，不会产生调度，因此，也不存在加入|删除队列的竞争
struct list_head timer_queue[NCPU];

struct thread_down
{
    struct thread_info *thread;
    semaphore_t sem;
};

inline void time_init()
{
    sys_ticks = 0;
    for (uint32 i = 0; i < NCPU; i++)
    {
        INIT_LIST_HEAD(&timer_queue[i]);
    }
}

// 我们以后改为线程池，暂时先用创建线程代替
// 且目前我们还没有写进程资源回收，也就是会越来越多
static void timer_wake(timer_t *t)
{
    if (t->block == TIMER_NO_BLOCK) // 不堵塞的话，直接使用工作队列就行
    {
        // printk("no_block -> work_queue\n");
        work_queue_push(t->callback, t->args);
    }
    else if (t->block == TIMER_BLOCK) // 会堵塞，则创建内核线程
    {
        // printk("block -> thread\n");
        kthread_create(t->callback, t->args, "ktimer", NO_CPU_AFF);
    }
    else
        printk("timer.c [timer_wake]: TIMER_NO_BLOCK?TIMER_BLOCK");
}

// 销毁内核定时器
static void timer_del(timer_t *t)
{
    kmem_cache_free(&timer_kmem_cache, t);
}

inline ticks_t get_cur_time()
{
    return sys_ticks;
}

// 每次时钟中断，都要检查是否到期
static void timer_try_wake()
{
    // printk("timer_try_wake\n");
    timer_t *t;
    timer_t *tmp;
    struct list_head *ct = &timer_queue[cpuid()];

    list_for_each_entry_safe(t, tmp, ct, list)
    {
        // printk("%d,%d\n", sys_ticks, t->init_time + t->during_time);
        if (sys_ticks >= t->init_time + t->during_time) // 到期了
        {
            t->init_time = sys_ticks;
            if (t->count != NO_RESTRICT)
            {
                t->count--;
                if (t->count == 0)
                {
                    list_del(&t->list);
                    timer_wake(t);
                    timer_del(t);
                    continue;
                }
            }
            timer_wake(t);
        }
    }
}
// 现在是在中断的情况下，是绝对不允许睡眠的
inline void time_update()
{
    // 这个就算改不及时也没事，也就是说 CPU0改了后，
    // 哪怕CPU1看到的是以前的，也就只差一个时钟中断而已，很快就来了。没必要使用自旋锁

    // if (cpuid() == 0)
    //     sys_ticks++;
    // timer_try_wake();

    if (cpuid() == 0)
        sys_ticks++;
    // TODO 每个cpu每3次唤醒一次，这样来说实际上是有点不能及时唤醒的，
    // TODO 应该会差 1,2或者更多个（如果此时cpu还在关中断）时钟周期，但是这样可以大大减少唤醒次数
    // TODO 这里没有必要对 sys_ticks 强加锁
    if (sys_ticks % 3 == 0)
        timer_try_wake();
}

static void assign_cpu(timer_t *t)
{
    struct list_head *ct = &timer_queue[cpuid()];
    list_add_head(&t->list, ct);
    // printk("timer: %p assign_cpu-> %d\n", t->callback, cpuid());
}

// 创建的内核定时器
timer_t *timer_create(void (*callback)(void *), void *args, uint64 during_time, int count, int is_block)
{
    timer_t *t = (timer_t *)kmem_cache_alloc(&timer_kmem_cache);
    if (!t)
    {
        printk("timer.c [timer_create]: Timer memory request failed\n");
        return NULL;
    }
    t->callback = callback;
    t->args = args;
    t->init_time = get_cur_time();
    t->during_time = during_time;
    t->count = count;
    t->block = is_block;
    assign_cpu(t);
    return t;
}

static void thread_time_up(void *args)
{
    struct thread_down *td = (struct thread_down *)args;
    sem_signal(&td->sem);
    kfree(td);
}

// 让线程自己堵塞 down_time，也就是 sleep
// 该函数会创建一个定时器执行 thread_time_up ，在一定时间后信号唤醒
// 唤醒函数只执行一次
void thread_timer_sleep(struct thread_info *thread, uint64 down_time)
{
    struct thread_down *td = (struct thread_down *)kmalloc(sizeof(struct thread_down), 0);
    sem_init(&td->sem, 0, "");
    td->thread = thread;
    timer_create(thread_time_up, td, down_time, 1, TIMER_NO_BLOCK);
    sem_wait(&td->sem);
}
