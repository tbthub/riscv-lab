#ifndef __PROC_H__
#define __PROC_H__

#include "std/stddef.h"
#include "core/sched.h"
#include "param.h"
#include "lib/list.h"
#include "lib/spinlock.h"
#include "riscv.h"

typedef int pid_t;
#define KERNEL_STACK_SIZE 4096
#define current myproc()

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
  struct sched_struct sched_list; // 每个CPU的进程调用链
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

#define T_READY (1 << 0)
struct thread_info
{
  struct task_struct *task;

  spinlock_t lock;

  // p->lock must be held when using these:
  uint64 signal;
  uint32 flags;
  enum task_state state;
  pid_t pid;
  uint64 ticks;

  // wait_lock must be held when using this:
  struct thread_info *parent; // 父进程指针
  struct list_head sched;     // 全局任务链表, 同时也是信号量等待队列

  void (*func)(void *);
  void *args;
  int cpu_affinity;

  uint16 cpu_id; // 用于记录当前线程在哪个核心上运行，这个暂时没想到干啥，先用着

  // these are private to the process, so p->lock need not be held.
  struct context context;
  char name[16];
};

extern void proc_init();
extern int cpuid();
extern struct cpu *mycpu();
extern struct thread_info *myproc(void);
extern struct thread_info *kthread_struct_init();
extern struct thread_info *uthread_struct_init();

#endif