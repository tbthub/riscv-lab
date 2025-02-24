#include "lib/spinlock.h"
#include "core/proc.h"
#include "core/sched.h"
#include "riscv.h"
#include "std/stddef.h"
#include "lib/atomic.h"
#include "lib/string.h"
#include "core/vm.h"

// 0号进程也就是第一个内核线程，负责初始化部分内容后作为调度器而存在

// 为每一个cpu分配一个调度队列
// 在顶层以后可以加一个负载均衡的分配器
// 负责分发不同的任务，现在为了简单就直接轮转加入

// 负载均衡器，这里暂时轮转分配(有点性能问题)
spinlock_t load_lock;
int load_cpu_id = -1;

struct thread_info *init_thread;
// switch.S
extern void swtch(struct context *, struct context *);

// 从调度队列中选择下一个要执行的线程
// 如果有其他进程，则选择其他的
static struct thread_info *pick_next_task(struct list_head *run_list)
{
    struct thread_info *thread;
    if (list_empty(run_list))
        return NULL;
    // printk("cpu%d: sched len:%d\n", cpuid(), list_len(run_list));
    list_for_each_entry_reverse(thread, run_list, sched)
    {
        // 找到一个可以运行的线程
        if (thread->state == RUNNABLE)
            return thread;
    }
    return NULL;
}

static void add_runnable_task(struct thread_info *thread)
{
    int cpuid;
    int cpu_affinity = thread->cpu_affinity;

    spin_lock(&load_lock);
    if (cpu_affinity == NO_CPU_AFF)
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

void sched(void)
{
    int intena;
    struct thread_info *thread = myproc();

    assert(holding(&thread->lock) != 0, "sched: not held p->lock");                                     // 确保持有锁
    assert(thread->state != RUNNING, "sched: '%s' is running, state: %d", thread->name, thread->state); // 确保进程不在运行态
    assert(intr_get() == 0, "sched interruptible");                                                     // 确保关中断

    intena = mycpu()->intena;
    // printk("Thread %s switch to scheduler in sched\n", thread->name);
    swtch(&thread->context, &mycpu()->context);
    spin_unlock(&thread->lock);
    // printk("Thread %s releasing lock on cpu %d ret sched\n", thread->name, cpuid());

    mycpu()->intena = intena;
}

// 从进程上下文切换到调度线程（主线程）, 进程交换上下文后中断返回
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
            // 设置 sscratch 为线程内核栈栈顶
            // w_sscratch((uint64)next + 2 * PGSIZE - 16);
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

void sched_init()
{
    spin_init(&load_lock, "load_lock");
    // 每个CPU都有一个指针指向这个idle
    for (int i = 0; i < NCPU; i++)
    {
        spin_init(&cpus[i].sched_list.lock, "sched_list");
        INIT_LIST_HEAD(&cpus[i].sched_list.run);
        INIT_LIST_HEAD(&cpus[i].sched_list.out);
    }
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

void kthread_create(void (*func)(void *), void *args, const char *name, int cpu_affinity)
{
    if (cpu_affinity < NO_CPU_AFF || cpu_affinity >= NCPU)
    {
        printk("Please pass in a suitable cpu_affinity value, "
               "the default value is 'NO_CPU_AFF'.\n"
               "The value you pass in is %d\n",
               cpu_affinity);
        return;
    }
    struct thread_info *t = kthread_struct_init();
    if (!t)
    {
        printk("kthread_create\n");
        return;
    }
    t->func = func;
    t->args = args;
    strncpy(t->name, name, 16);
    t->cpu_affinity = cpu_affinity;

    wakeup_process(t);
}

static uchar initcode[] = {0x5d, 0x71, 0x86, 0xe4, 0xa2, 0xe0, 0x80, 0x08, 0x23, 0x26, 0x04, 0xfe, 0x97, 0x07, 0x00, 0x00, 
    0x93, 0x87, 0x47, 0x11, 0x23, 0x30, 0xf4, 0xfe, 0x85, 0x47, 0x86, 0x17, 0x23, 0x3c, 0xf4, 0xfc, 
    0x97, 0x07, 0x00, 0x00, 0x93, 0x87, 0x07, 0x11, 0x23, 0x38, 0xf4, 0xfa, 0x97, 0x07, 0x00, 0x00, 
    0x93, 0x87, 0xc7, 0x10, 0x23, 0x3c, 0xf4, 0xfa, 0x23, 0x30, 0x04, 0xfc, 0x91, 0x47, 0x23, 0x38, 
    0xf4, 0xfc, 0xfd, 0x57, 0x23, 0x26, 0xf4, 0xfc, 0x83, 0x27, 0xc4, 0xfc, 0x93, 0x06, 0x04, 0xfb, 
    0x03, 0x25, 0xc4, 0xfe, 0x03, 0x37, 0x04, 0xfd, 0x03, 0x36, 0x84, 0xfd, 0x83, 0x35, 0x04, 0xfe, 
    0xef, 0x00, 0x60, 0x00, 0xd5, 0xb7, 0x81, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x85, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x89, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x8d, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x91, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x95, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xc1, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xd5, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x99, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x9d, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xbd, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xc5, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xc9, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xa1, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xcd, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xd1, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xa5, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xa9, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xad, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xb1, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xb5, 0x48, 0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0xb9, 0x48, 
    0x73, 0x00, 0x00, 0x00, 0x82, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x31, 0x20, 0x61, 0x72, 0x67, 0x73, 0x00, 
    0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0x00, 0x00, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00, 
};

// 当切换到 forkret 的时候，这里的 sp 是为0，但是此时还处于用户模式，
// 内核中如发生中断，则栈指针会有问题。
// 因此，初始化用户程序的时候必须要关中断的情况下进行
void user_init()
{
    init_thread = uthread_struct_init();

    init_thread->cpu_affinity = NO_CPU_AFF;

    init_thread->tf->kernel_sp = Kernel_stack_top(init_thread);
    init_thread->task->pagetable = alloc_pt();
    init_thread->task->sz = PGSIZE;

    uvmfirst(init_thread, initcode, sizeof(initcode));

    strncpy(init_thread->name, "initcode", sizeof(init_thread->name));
    wakeup_process(init_thread);
}

__attribute__((unused)) void debug_cpu_shed_list()
{
    printk("\nCPU shed list:\n");
    for (int i = 0; i < NCPU; i++)
    {
        printk("cpu %d len %d\n", i, list_len(&cpus[i].sched_list.run));
    }
}
