#include "riscv.h"
#include "mm/slab.h"
#include "core/sched.h"
#include "std/stdarg.h"
#include "mm/kmalloc.h"

#include "lib/list.h"
#include "lib/string.h"

#include "core/vm.h"
#include "core/proc.h"

static struct
{
  // 用于分配 pid 由于我们使用了slab回收的可以直接利用
  int pids;
  // 保护pids的递增
  spinlock_t lock;
} pid_pool;

struct cpu cpus[NCPU];
extern void usertrapret(uint64 spec);

// 退出（ZOMBIE）后重新调度
static void quit()
{
  struct cpu *cpu = mycpu();
  struct thread_info *thread = cpu->thread;

  spin_lock(&thread->lock);

  thread->state = ZOMBIE;

  list_add_tail(&thread->sched, &cpu->sched_list.out);

  sched();
}

// 线程出口函数，由 thread_entry 执行完 func 后调用
static void thread_exit()
{
  struct thread_info *thread = myproc();

  // 由父进程来回收的
  // kmem_cache_free(&task_struct_kmem_cache, current_->task);
  // kmem_cache_free(&thread_info_kmem_cache, current_);
  printk("thread: %s exit!\n", thread->name);

  // 需要在关中断的情况下切换进入调度器上下文
  intr_off();
  quit();
}

// 线程入口函数，用于执行传入的线程函数
static void thread_entry()
{
  struct thread_info *thread = myproc();
  // 第一次需要释放锁
  // printk("Thread %s release lock on cpu %d in thread_entry  --only once\n", thread->name, cpuid());
  spin_unlock(&thread->lock);
  intr_on(); // 需要开中断。由时钟中断->调度器切换->是关了中断的，要重新打开
  thread->func(thread->args);
  thread_exit();
}

// 申请一个空白的PCB，包括 thread_info + task_struct
static struct thread_info *alloc_thread()
{
  struct task_struct *task = kmem_cache_alloc(&task_struct_kmem_cache);
  if (!task)
    return NULL;
  struct thread_info *thread = kmem_cache_alloc(&thread_info_kmem_cache);
  if (!thread)
  {
    kmem_cache_free(&task_struct_kmem_cache, task);
    return NULL;
  }

  spin_init(&thread->lock, "thread");

  thread->task = task;
  thread->signal = 0;
  thread->flags = 0;
  thread->state = USED;

  spin_lock(&pid_pool.lock);
  thread->pid = thread->pid == 0 ? pid_pool.pids++ : thread->pid;
  spin_unlock(&pid_pool.lock);

  thread->ticks = 10;
  thread->args = NULL;
  thread->func = NULL;
  // thread->cpu_affinity = NO_CPU_AFF;

  INIT_LIST_HEAD(&thread->sched);
  memset(&thread->context, 0, sizeof(thread->context));
  // thread->context.ra = NULL;
  // thread->context.sp = (uint64)thread + 2 * PGSIZE - 1;

  spin_init(&thread->task->lock, "thread");
  thread->task->pagetable = NULL;
  thread->task->sz = 0;
  return thread;
}

// 这里应该是从 scheduler 的 swtch 进入
// 在 scheduler 中获取了锁，这里需要释放
static void forkret()
{
  spin_unlock(&myproc()->lock);
  usertrapret(USER_TEXT_BASE);
}

struct thread_info *kthread_struct_init()
{
  struct thread_info *t = alloc_thread();
  if (!t)
  {
    printk("kthread_struct_init error!\n");
    return NULL;
  }
  t->context.ra = (uint64)thread_entry;
  t->context.sp = (uint64)t + 2 * PGSIZE - 16;
  return t;
}

struct thread_info *uthread_struct_init()
{
  struct thread_info *t = alloc_thread();
  if (!t)
  {
    printk("uthread_struct_init error!\n");
    return NULL;
  }

  t->context.ra = (uint64)forkret;
  // 上下文的这个设置成经过中断压栈后的地址
  t->context.sp = (uint64)t + 2 * PGSIZE - 16 - 248;
  return t;
}

// 获取 cpuid
// * 必须在关中断环境下,防止与进程移动发生竞争，到不同的CPU。
inline int cpuid()
{
  return r_tp();
}

// 返回该 CPU 的 cpu 结构体。
// * 必须在关中断环境下。
inline struct cpu *mycpu()
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

struct thread_info *myproc()
{
  push_off();
  struct thread_info *thread = mycpu()->thread;
  pop_off();
  return thread;
}

void proc_init()
{
  spin_init(&pid_pool.lock, "pid_pool");
  pid_pool.pids = 0;
}
