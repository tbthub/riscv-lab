#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
// 配置内核的异常和中断向量

// 设置异常和中断的处理程序入口，具体是将 kernelvec 函数的地址写入到 stvec 寄存器中。
// stvec 是用于在 RISC-V 中存储异常/中断向量表的寄存器。
// 在 RISC-V 中，当发生异常或中断时，CPU 会自动将控制权转交给 stvec 中存储的地址
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
// user下 interrupt, exception, or system call 处理的统一入口
// 因此当处于用户态运行时 stvec 寄存器存放 uservec 就是这玩意儿
void usertrap(void)
{
  int which_dev = 0;

  // 检查是否来自用户模式下的中断，也就是不是内核中断
  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  // 现在位于内核，要设置 stvec 为 kernelvec
  w_stvec((uint64)kernelvec);

  // (位于进程上下文的，别忘记了:-)
  struct proc *p = myproc();

  // save user program counter.
  // 读取保存以前的 pc
  p->trapframe->epc = r_sepc();

  // 查看 scause 寄存器，确定发生 trap 的原因
  if (r_scause() == 8)
  {
    // system call

    if (killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    // 注：syscall 可以被中断，上面打开了
    syscall();
  }

  // 下面都是保持关中断继续运行的
  else if ((which_dev = devintr()) != 0)
  { // 1.virio 2.timer 0.others
    // ok
  }
  else
  {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if (killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
  // 就算是进程切换，现在我们又来到了新的进程，处于新的进程上下文
  // 如果不涉及进程切换，我们还是在原来的进程上下文
  // 我胡汉三又回来拉
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  // 由于下面的东西需要关中断运行（要操作一些核心寄存器），而system_call这个独特的家伙允许开中断
  // 因此我们统一再次关下
  intr_off();

  // 下面是开始恢复一些东西

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
// 内核中的处理函数 kerneltrap
void kerneltrap()
{
  int which_dev = 0;            // 哪一个设备
  uint64 sepc = r_sepc();       // sepc: 保存异常发生时的 sepc 寄存器的值，sepc 存储了引发异常或中断时的程序计数器（PC）的值
  uint64 sstatus = r_sstatus(); // sstatus: 保存当前的 sstatus 寄存器的值，sstatus 包含状态标志，例如当前的运行模式
  // uint64 scause = r_scause();   // scause: 保存 scause 寄存器的值，scause 指示了异常或中断的原因

  // if ((sstatus & SSTATUS_SPP) == 0)
  //   panic("kerneltrap: not from supervisor mode");
  // if (intr_get() != 0) // xv6不允许嵌套中断，因此执行到这里的时候一定是关中断的
  //   panic("kerneltrap: interrupts enabled");

  if ((which_dev = devintr()) == 0)
  { // 处理设备中断
    // interrupt or trap from an unknown source
    // printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    // panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.处理时钟中断（让出 CPU）
  // if (which_dev == 2 && myproc() != 0){}
    // yield();

  // yield() 可能会导致一些新的异常或中断，因此在返回之前，需要恢复原来的 sepc 和 sstatus 值。
  // w_sepc(sepc): 将原来的 sepc 值写回寄存器，以便异常返回时能够继续原来的代码执行。
  // w_sstatus(sstatus): 恢复 sstatus 的状态，以确保内核的状态和之前一致。
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr()
{
  // if (cpuid() == 0)
  // {
  //   acquire(&tickslock);
  //   ticks++;
  //   // wakeup(&ticks);
  //   release(&tickslock);
  // }

  // // ask for the next timer interrupt. this also clears
  // // the interrupt request. 1000000 is about a tenth
  // // of a second.
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
  uint64 scause = r_scause();

  if (scause == 0x8000000000000009L)
  {
    uartputc_sync('D');
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();
    // plic_claim();
    if (irq == UART0_IRQ)
    {
      // uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000005L)
  {
    // timer interrupt.
    clockintr();
    // w_stimecmp(r_time() + 1000000);
    // uartputc_sync('C');
    return 2;
  }
  else
  {
    return 0;
  }
}
