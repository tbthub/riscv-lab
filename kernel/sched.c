#include "utils/spinlock.h"
#include "core/proc.h"
#include "core/sched.h"
#include "riscv.h"
#include "std/stddef.h"
#include "utils/atomic.h"

// 0号进程也就是第一个内核线程，负责初始化部分内容后作为调度器而存在

// 为每一个cpu分配一个调度队列
// 在顶层以后可以加一个负载均衡的分配器
// 负责分发不同的任务，现在为了简单就直接轮转加入
// 每个CPU不可能访问其他CPU的队列，因此不必加锁

// switch.S
void swtch(struct context *, struct context *);

// 从调度队列中选择下一个要执行的线程
// 如果有其他进程，则选择其他的
struct thread_info *pick_next_task(struct list_head *run_list)
{
    // printk("SCHED RUNANLE: %d\n", list_len(&mycpu()->sched_list.run));
    struct thread_info *thread;
    if (list_empty(run_list))
        return NULL;
    list_for_each_entry(thread, run_list, sched)
    {
        // 找到一个可以运行的线程
        if (thread->state == RUNNABLE)
            return thread;
    }
    return NULL;
}

// 负载均衡器，这里暂时轮转分配(有点性能问题)
spinlock_t load_lock;
int load_cpu_id = -1;

void add_runnable_task(struct thread_info *thread)
{
    int cpuid;
    int cpu_affinity = thread->cpu_affinity;

    spin_lock(&load_lock);
    if (cpu_affinity == no_cpu_affinity)
        cpuid = load_cpu_id = (load_cpu_id + 1) % NCPU;
    else
        cpuid = cpu_affinity;
#ifdef DEBUG_TASK_ADD_CPU
    printk("add thread %s to cpu %d\n", thread->name, cpuid);
#endif
    spin_unlock(&load_lock);

    spin_lock(&cpus[cpuid].sched_list.lock);
    list_add_head(&thread->sched, &(cpus[cpuid].sched_list.run));
    spin_unlock(&cpus[cpuid].sched_list.lock);
}

void debug_cpu_shed_list()
{
    printk("\nCPU shed list:\n");
    for (int i = 0; i < NCPU; i++)
    {
        printk("cpu %d len %d\n", i, list_len(&cpus[i].sched_list.run));
    }
}

void scheduler()
{
    struct cpu *cpu = mycpu();

    intr_on();
    while (1)
    {
        spin_lock(&cpu->sched_list.lock);
        struct thread_info *next = pick_next_task(&cpu->sched_list.run);
        if (next)
        {
            // 找到下一个线程，从就绪列表摘下
            // 原则上每个CPU有自己的调度队列，不会出现多个CPU同时调度一个线程的情况
            // 但是我们走 sleep 时候要主动让出CPU，在信号量中有对进程加锁，因此这里交换回来要解锁
            // 同理,我们在交换出去的时候加锁
            spin_lock(&next->lock);
            // printk("Thread %s acquiring lock on cpu %d in scheduler\n", next->name, cpuid());
            // printk("pick thread name: %s\n", next->name);
            list_del(&next->sched);
            spin_unlock(&cpu->sched_list.lock);

            next->state = RUNNING;
            next->cpu_id = cpuid();
            cpu->thread = next;
#ifdef DEBUG_TASK_ON_CPU
            printk("thread: %s in running on hart %d\n", next->name, cpuid());
#endif
            // printk("switch to thread: %s ra: %p sp: %p\n", next->name, next->context.ra, next->context.sp);
            swtch(&cpu->context, &next->context);

            // 线程已经在运行了
            // printk("thread: %s return..scheduler, intr: %d\n",next->name,intr_get());
            // 下面是被调度返回了调度器线程
            spin_unlock(&next->lock);
            // printk("Thread %s releasing lock on cpu %d in scheduler,intr: %d\n", next->name, cpuid(),intr_get());
            cpu->thread = NULL;
        }
        // 如果没有一个可以运行的进程，则运行idle
        else
        {
            spin_unlock(&cpu->sched_list.lock);
            intr_on();
            asm volatile("wfi");
        }
    }
}

void sched(void)
{
    int intena;
    struct thread_info *thread = myproc();

    if (!holding(&thread->lock))
        panic("sched p->lock");
    if (thread->state == RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    // printk("Thread %s switch to scheduler in sched\n", thread->name);
    swtch(&thread->context, &mycpu()->context);
    spin_unlock(&thread->lock);
    // printk("Thread %s releasing lock on cpu %d ret sched\n", thread->name, cpuid());

    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
// 从进程上下文切换到调度线程（主线程）
// 进程交换上下文后中断返回
void yield()
{
    // printk("-----------timer interrupt yield!!!--------------\n");
    struct thread_info *thread = myproc();

    // printk("Thread %s acquiring lock on cpu %d in yield\n", thread->name, cpuid());
    spin_lock(&thread->lock);
    thread->ticks = 10;
    thread->state = RUNNABLE;
    add_runnable_task(thread);
    // list_add_tail(&thread->sched, &mycpu()->sched_list.run);
    sched();
}

void quit()
{
    struct thread_info *thread = myproc();
    struct cpu *cpu = mycpu();
    spin_lock(&thread->lock);
    thread->state = ZOMBIE;
    list_add_tail(&thread->sched, &cpu->sched_list.out);
    sched();
}

void wakeup_process(struct thread_info *thread)
{
    // printk("Thread %s acquiring lock on cpu %d in wakeup\n", thread->name, cpuid());
    spin_lock(&thread->lock);
    thread->state = RUNNABLE;
    add_runnable_task(thread);
    spin_unlock(&thread->lock);
    // printk("Thread %s releasing lock on cpu %d in wakeup\n", thread->name, cpuid());
}
