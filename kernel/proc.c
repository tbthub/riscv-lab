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
extern void usertrapret() __attribute__((noreturn));

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

// 初始化 task_struct
static inline void task_struct_init(struct task_struct *task)
{
  spin_init(&task->lock, "task");
  memset(&task->mm, 0, sizeof(task->mm));
  spin_init(&task->mm.lock, "mm_struct");
}

static inline void thread_info_init(struct thread_info *thread)
{
  thread->task = NULL;
  spin_init(&thread->lock, "thread");
  thread->signal = 0;
  thread->flags = 0;
  // thread->state = UNUSED;
  // thread->pid = -1;
  
  thread->tf = NULL;
  thread->ticks = 10;
  thread->args = NULL;
  thread->func = NULL;
  thread->cpu_affinity = NO_CPU_AFF;

  INIT_LIST_HEAD(&thread->sched);
  memset(&thread->context, 0, sizeof(thread->context));

  thread->context.sp = Kernel_stack_top(thread);
}

// 这里应该是从 scheduler 的 swtch 进入
// 在 scheduler 中获取了锁，这里需要释放
static void forkret()
{
  spin_unlock(&myproc()->lock);
  usertrapret();
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
  task_struct_init(task);
  thread_info_init(thread);
  thread->task = task;

  spin_lock(&pid_pool.lock);
  thread->pid = thread->pid == 0 ? pid_pool.pids++ : thread->pid;
  spin_unlock(&pid_pool.lock);

  return thread;
}

struct thread_info *alloc_kthread()
{
  struct thread_info *t = alloc_thread();
  if (!t)
  {
    printk("alloc_kthread error!\n");
    return NULL;
  }
  t->context.ra = (uint64)thread_entry;
  return t;
}

struct thread_info *alloc_uthread()
{
  struct thread_info *t = alloc_thread();
  if (!t)
  {
    printk("alloc_uthread: error!\n");
    return NULL;
  }
  t->tf = kmem_cache_alloc(&tf_kmem_cache);
  if (!t->tf)
  {
    panic("alloc_uthread: tf_kmem_cache\n");
    // printk("alloc_uthread tf_kmem_cache\n");
    // TODO 添加回收的逻辑，不过我们暂时先这样，不考虑异常，直接堵死
  }
  t->context.ra = (uint64)forkret;
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

//  TODO 检查地址合法性
// int is_user_address(struct mm_struct *mm, uint64_t va) {
//   return (va >= mm->start_addr && va < mm->end_addr);
// }
