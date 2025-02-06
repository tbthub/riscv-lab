#include "riscv.h"
#include "std/stdio.h"
#include "core/proc.h"
#include "core/trap.h"
#include "core/sched.h"
#include "dev/plic.h"
#include "mm/memlayout.h"
#include "dev/uart.h"
#include "core/timer.h"
extern void virtio_disk_intr();
// in kernelvec.S, calls kerneltrap().
void kernelvec();

void trap_init()
{
    time_init();
}

// set up to take exceptions and traps while in the kernel.
// 配置内核的异常和中断向量

// 设置异常和中断的处理程序入口，具体是将 kernelvec 函数的地址写入到 stvec 寄存器中。
// stvec 是用于在 RISC-V 中存储异常/中断向量表的寄存器。
// 在 RISC-V 中，当发生异常或中断时，CPU 会自动将控制权转交给 stvec 中存储的地址
void trap_inithart(void)
{
    w_stvec((uint64)kernelvec);
}

void do_timer()
{

    time_update();

    struct thread_info *cur = current;
    if (!cur)
    {
        // 当前没有进程，正在开中断的情况下等待
        w_stimecmp(r_time() + 8000);
        return;
    }

    // spin_lock(&cur->lock);
    // ticks 为线程的私有变量，不需要加锁
    cur->ticks--;
    // spin_unlock(&cur->lock);

    // printk("timer");
    w_stimecmp(r_time() + 500000);
}
void do_external()
{
    int irq = plic_claim();
    if (irq == UART0_IRQ)
    {
        uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
        virtio_disk_intr();
    }
    else if (irq)
    {
        printk("unexpected interrupt irq=%d\n", irq);
    }
    if (irq)
        plic_complete(irq);
}

int dev_intr()
{
    uint64 scause = r_scause();
    switch (scause)
    {
    case EXTERNAL_SCAUSE:
        do_external();
        return 1;

    case TIMER_SCAUSE:
        do_timer();
        return 2;

    default:
        // printk("unkown dev\n");
        return 0;
    }
}

// 内核中的处理函数 kernel_trap
void kerneltrap()
{
    int which_dev = 0;            // 哪一个设备
    uint64 sepc = r_sepc();       // sepc: 保存异常发生时的 sepc 寄存器的值，sepc 存储了引发异常或中断时的程序计数器（PC）的值
    uint64 sstatus = r_sstatus(); // sstatus: 保存当前的 sstatus 寄存器的值，sstatus 包含状态标志，例如当前的运行模式
    uint64 scause = r_scause();   // scause: 保存 scause 寄存器的值，scause 指示了异常或中断的原因

    if ((sstatus & SSTATUS_SPP) == 0)
    {
        panic("kerneltrap: not from supervisor mode %d", sstatus & SSTATUS_SPP);
    }

    if (intr_get() != 0) // xv6不允许嵌套中断，因此执行到这里的时候一定是关中断的
        panic("kerneltrap: interrupts enabled");
    uint64 stval;
    if ((which_dev = dev_intr()) == 0)
    {
        switch (scause)
        {
        case 12:
            printk("kerneltrap scause: 12\n");
            asm volatile("csrr %0, scause" : "=r"(scause));
            asm volatile("csrr %0, stval" : "=r"(stval));
            printk("scause: %p, stval: %p\n", scause, stval);
            break;
        case 13:
            printk("kerneltrap scause: 13 (Load Page Fault)\n");
            asm volatile("csrr %0, scause" : "=r"(scause));
            asm volatile("csrr %0, stval" : "=r"(stval));
            printk("scause: %p, stval: %p\n", scause, stval);
            break;
        case 15:
            printk("kerneltrap scause: 15 (Store/AMO Page Fault)\n");
            asm volatile("csrr %0, scause" : "=r"(scause));
            asm volatile("csrr %0, stval" : "=r"(stval));
            printk("scause: %p, stval: %p\n", scause, stval);
            break;
        default:
            break;
        }
    }
    // if ((which_dev = dev_intr()) == 0)
    // {
    //     // interrupt or trap from an unknown source
    //     printk("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    //     panic("kerneltrap");
    // }

    // give up the CPU if this is a timer interrupt.处理时钟中断（让出 CPU）
    if (which_dev == 2 && myproc() != 0 && myproc()->ticks == 0)
        yield();

    // yield() 可能会导致一些新的异常或中断，因此在返回之前，需要恢复原来的 sepc 和 sstatus 值。
    // w_sepc(sepc): 将原来的 sepc 值写回寄存器，以便异常返回时能够继续原来的代码执行。
    // w_sstatus(sstatus): 恢复 sstatus 的状态，以确保内核的状态和之前一致。

    w_sepc(sepc);
    w_sstatus(sstatus);
}
