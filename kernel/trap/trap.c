#include "riscv.h"
#include "std/stdio.h"
#include "core/proc.h"
#include "core/trap.h"
#include "core/sched.h"
#include "dev/plic.h"
#include "mm/memlayout.h"
#include "dev/uart.h"
#include "core/timer.h"
#include "lib/semaphore.h"
extern void virtio_disk_intr();
// in kernelvec.S, calls kerneltrap().
extern void kernelvec();
extern void uservec();
extern void userret() __attribute__((noreturn));
extern void syscall();

void trap_init()
{
    time_init();
}

// 配置内核的异常和中断向量

// 设置异常和中断的处理程序入口，具体是将 kernelvec 函数的地址写入到 stvec 寄存器中。
// stvec 是用于在 RISC-V 中存储异常/中断向量表的寄存器。
// 在 RISC-V 中，当发生异常或中断时，CPU 会自动将控制权转交给 stvec 中存储的地址
void trap_inithart(void)
{
    w_stvec((uint64)kernelvec);
}

static void timer_intr()
{

    time_update();

    struct thread_info *cur = myproc();
    if (!cur)
    {
        // 当前没有进程，正在开中断的情况下等待
        w_stimecmp(r_time() + 8000);
        return;
    }

    // ticks 为线程的私有变量，不需要加锁
    cur->ticks--;

    w_stimecmp(r_time() + 250000);

    // 到期就让出 CPU
    if (cur->ticks == 0)
        yield();
}

static inline void external_intr()
{
    int irq = plic_claim();
    switch (irq)
    {
    case UART0_IRQ:
        uartintr();
        break;
    case VIRTIO0_IRQ:
        virtio_disk_intr();
        break;
    case 0:
        break;
    default:
        printk("unexpected interrupt irq=%d\n", irq);
        break;
    }

    if (irq)
        plic_complete(irq);
}

static inline void intr_handler(uint64 scause)
{
    switch (scause)
    {
    case EXTERNAL_SCAUSE: // 外设
        external_intr();
        break;

    case TIMER_SCAUSE: // 时钟
        timer_intr();
        break;

    default:
        break;
    }
}

#define E_SYSCALL 8L       // 系统调用
#define E_INS_PF 12L       // 指令缺页
#define E_LOAD_PF 13L      // 加载缺页
#define E_STORE_AMO_PF 15L // 存储缺页

static void excep_handler(uint64 scause)
{
    // uint64 stval;
    switch (scause)
    {
    case E_SYSCALL:
        intr_on();
        syscall();
        break;

    case E_INS_PF:
    case E_LOAD_PF:
    case E_STORE_AMO_PF:

        break;
    default:
        printk("unknown scause: %p\n", scause);
        break;
    }
}

__attribute__((noreturn)) void usertrapret()
{
    struct thread_info *p = myproc();
    // 我们即将把陷阱的目标从
    // kerneltrap() 切换到 usertrap()，因此请关闭中断，直到
    // 我们回到用户空间，此时 usertrap() 是正确的。(系统调用之前会开中断)
    intr_off();

    w_sepc(p->tf->epc);
    w_stvec((uint64)uservec);
    w_sscratch((uint64)(p->tf));

    // set S Previous Privilege mode to User.
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);

    spin_lock(&p->task->mm.lock);
    // 如果发生了进程切换
    if (r_satp() != MAKE_SATP(p->task->mm.pgd))
    {
        sfence_vma();
        w_satp(MAKE_SATP(p->task->mm.pgd));
        sfence_vma();
    }
    spin_unlock(&p->task->mm.lock);
    userret();
}

// 内核 trap 处理函数 kernel_trap
void kerneltrap()
{
    uint64 sepc = r_sepc();       // sepc: 保存异常发生时的 sepc 寄存器的值，sepc 存储了引发异常或中断时的程序计数器（PC）的值
    uint64 sstatus = r_sstatus(); // sstatus: 保存当前的 sstatus 寄存器的值，sstatus 包含状态标志，例如当前的运行模式
    uint64 scause = r_scause();   // scause: 保存 scause 寄存器的值，scause 指示了异常或中断的原因

    assert((sstatus & SSTATUS_SPP) != 0, "kerneltrap: not from supervisor mode %d", sstatus & SSTATUS_SPP);
    assert(intr_get() == 0, "kerneltrap: interrupts enabled"); // xv6不允许嵌套中断，因此执行到这里的时候一定是关中断的

    if (scause & (1ULL << 63))
        intr_handler(scause);
    else
        excep_handler(scause);

    w_sepc(sepc);       // 将原来的 sepc 值写回寄存器，以便异常返回时能够继续原来的代码执行。
    w_sstatus(sstatus); //  恢复 sstatus 的状态，以确保内核的状态和之前一致。
}

// 用户 trap 处理函数 user_trap
void usertrap()
{
    uint64 scause = r_scause();
    uint64 sepc = r_sepc();

    // 检查是否来自用户模式下的中断，也就是不是内核中断,确保中断来自用户态
    assert((r_sstatus() & SSTATUS_SPP) == 0, "usertrap: not from user mode\n");
    assert(intr_get() == 0, "usertrap: interrupts enabled"); // xv6不允许嵌套中断，因此执行到这里的时候一定是关中断的

    // 现在位于内核，要设置 stvec 为 kernelvec
    w_stvec((uint64)kernelvec);

    // (位于进程上下文的，别忘记了:-)
    struct thread_info *p = myproc();
    assert(p != NULL, "usertrap: p is NULL\n");

    if (scause & (1ULL << 63))
        intr_handler(scause);
    else
    {
        // 额外处理下如果是系统调用
        if (scause == E_SYSCALL)
            // 返回到系统调用的下一条指令，即越过 ecall
            sepc += 4;
        excep_handler(scause);
    }

    p->tf->epc = sepc;
    usertrapret();
}