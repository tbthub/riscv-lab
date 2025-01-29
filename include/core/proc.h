#ifndef __PROC_H__
#define __PROC_H__

#include "std/stddef.h"
#include "core/sched.h"
#include "param.h"
#include "utils/list.h"
#include "utils/spinlock.h"
#include "riscv.h"

typedef int pid_t;
#define KERNEL_STACK_SIZE 4096
#define current myproc()
#define no_cpu_affinity -1

// Saved registers for kernel context switches.
struct context
{
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu
{

  struct context context; // swtch() here to enter scheduler().
  int noff;               // Depth of push_off() nesting.
  int intena;             // Were interrupts enabled before push_off()?

  // xv6中使用thread是为了方便找到当前进程。但是我们采取内核栈与PCB共占一个页面，也就是sp指针来确定即可
  // 暂时先不管
  struct thread_info *thread;
  struct thread_info *idle;
  struct sched_struct sched_list; // 每个CPU的进程调用链，不必加锁。
};
extern struct cpu cpus[NCPU];

// 线程状态
enum task_state
{
  UNUSED,
  USED,
  SLEEPING,
  RUNNABLE,
  RUNNING,
  ZOMBIE,
  IDLE
};

// 主要管理资源，文件等。对个thread_info对应一个task_struct。操作task_struct时候需要加锁
struct task_struct
{
  spinlock_t lock;

  pagetable_t pagetable;
  uint64 sz;

  // struct file *ofile[NOFILE];
  // struct inode *cwd;
};

struct thread_info
{
  struct task_struct *task;

  spinlock_t lock;

  // p->lock must be held when using these:
  uint64 signal;
  enum task_state state;
  pid_t pid;
  uint64 ticks;

  // wait_lock must be held when using this:
  struct list_head parent; // 父进程指针
  union
  {
    struct list_head sched;  // 全局任务链表
    struct list_head waiter; // 等待链表
  };
  union
  {
    struct trapframe *trapframe;
    void (*func)(void*);
  };
  void *args;
  uint16 cpu_id;   // 用于记录当前线程在哪个核心上运行
  int cpu_affinity;

  // these are private to the process, so p->lock need not be held.
  struct context context;
  char name[16];
};



struct pid_pool_struct
{
  // 用于分配 pid 由于我们使用了slab回收的可以直接利用
  int pids;
  // 保护pids的递增
  spinlock_t lock;
};

extern struct pid_pool_struct pid_pool;

extern void sched_init();
extern int cpuid();
extern struct cpu *mycpu();
extern struct thread_info *myproc(void);
extern struct thread_info *alloc_thread();
extern void thread_create(void (*func)(void *), void *args, const char *name);
extern void thread_create_affinity(void (*func)(void *), void *args, const char *name, int cpu_affinity);
extern void add_runnable_task(struct thread_info *thread);

#endif