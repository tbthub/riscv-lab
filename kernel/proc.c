#include "core/proc.h"
#include "riscv.h"
#include "mm/slab.h"
#include "utils/string.h"
#include "core/sched.h"
#include "std/stdarg.h"
#include "utils/list.h"
#include "mm/kmalloc.h"

struct pid_pool_struct pid_pool;
extern spinlock_t load_lock;

void swtch(struct context *, struct context *);

struct cpu cpus[NCPU];

// 必须在禁用中断的情况下调用，
// 防止与进程移动发生竞争，到不同的CPU。
int cpuid()
{
  int id = r_tp();
  return id;
}

// 返回该 CPU 的 cpu 结构体。
// 必须禁用中断。
inline struct cpu *mycpu()
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

struct thread_info *myproc(void)
{
  push_off();
  struct thread_info *thread = mycpu()->thread;
  pop_off();
  return thread;
}

void thread_exit()
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
void thread_entry()
{
  struct thread_info *thread = myproc();
  // 第一次需要释放锁
  // printk("Thread %s release lock on cpu %d in thread_entry  --only once\n", thread->name, cpuid());
  spin_unlock(&thread->lock);
  intr_on(); // 需要开中断。由时钟中断->调度器切换->是关了中断的，要重新打开
  thread->func(thread->args);
  thread_exit();
}

static inline void set_cpu_affinity(struct thread_info *thread, int cpu_id)
{
  thread->cpu_affinity = cpu_id;
}

void thread_create_affinity(void (*func)(void *), void *args, const char *name, int cpu_affinity)
{
  struct thread_info *new_thread;
  new_thread = alloc_thread();
  if (!new_thread)
    panic("thread_create error!\n");

  new_thread->state = RUNNABLE;
  strncpy(new_thread->name, name, 16);
  new_thread->context.ra = (uint64)thread_entry;
  new_thread->func = func;
  new_thread->args = args;
  set_cpu_affinity(new_thread, cpu_affinity);
  // printk("Thread created: %s\n", name);
  // 加入到CPU的调度队列中
  add_runnable_task(new_thread);
}
// 我们的内核线程暂时不加参数，以后再说
// 线程创建函数
void thread_create(void (*func)(void *), void *args, const char *name)
{
  struct thread_info *new_thread;
  new_thread = alloc_thread();
  if (!new_thread)
    panic("thread_create error!\n");

  new_thread->state = RUNNABLE;
  strncpy(new_thread->name, name, 16);
  new_thread->context.ra = (uint64)thread_entry;
  new_thread->func = func;
  new_thread->args = args;
  // printk("Thread created: %s\n", name);
  // 加入到CPU的调度队列中
  add_runnable_task(new_thread);
}

// 为每一个CPU都加上一e个idle进程
// 只能由一个CPU来调用
void sched_init()
{
  spin_init(&pid_pool.lock, "pid_pool");
  spin_init(&load_lock, "load_lock");
  pid_pool.pids = 0;

  // 每个CPU都有一个指针指向这个idle
  for (int i = 0; i < NCPU; i++)
  {
    spin_init(&cpus[i].sched_list.lock, "sched_list");
    INIT_LIST_HEAD(&cpus[i].sched_list.run);
    INIT_LIST_HEAD(&cpus[i].sched_list.out);
    // cpus[i].idle = idle_thread;
  }
}

// 申请一个空白的PCB，包括 thread_info + task_struct
struct thread_info *alloc_thread()
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
  thread->state = USED;

  spin_lock(&pid_pool.lock);
  thread->pid = thread->pid == 0 ? pid_pool.pids++ : thread->pid;
  spin_unlock(&pid_pool.lock);

  thread->ticks = 10;
  thread->args = NULL;
  thread->func = NULL;
  thread->cpu_affinity = no_cpu_affinity;

  INIT_LIST_HEAD(&thread->sched);
  memset(&thread->context, 0, sizeof(thread->context));
  // thread->context.ra = NULL;
  thread->context.sp = (uint64)thread + 2 * PGSIZE - 1;

  spin_init(&thread->task->lock, "thread");
  thread->task->pagetable = NULL;
  thread->task->sz = 0;
  return thread;
}
