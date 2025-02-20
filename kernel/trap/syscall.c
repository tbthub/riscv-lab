#include "core/syscall.h"
#include "core/proc.h"
#include "std/stddef.h"

extern int sys_debug();
extern int sys_fork();
extern int sys_exit();
extern int sys_wait();
extern int sys_pipe();
extern int sys_read();
extern int sys_kill();
extern int sys_exec();
extern int sys_fstat();
extern int sys_chdir();
extern int sys_dup();
extern int sys_getpid();
extern int sys_sbrk();
extern int sys_sleep();
extern int sys_uptime();
extern int sys_open();
extern int sys_write();
extern int sys_mknod();
extern int sys_unlink();
extern int sys_link();
extern int sys_mkdir();
extern int sys_close();

static int (*syscalls[])(void) = {
    [SYS_debug] sys_debug,
    [SYS_fork] sys_fork,
    [SYS_exit] sys_exit,
    [SYS_wait] sys_wait,
    [SYS_pipe] sys_pipe,
    [SYS_read] sys_read,
    [SYS_kill] sys_kill,
    [SYS_exec] sys_exec,
    [SYS_fstat] sys_fstat,
    [SYS_chdir] sys_chdir,
    [SYS_dup] sys_dup,
    [SYS_getpid] sys_getpid,
    [SYS_sbrk] sys_sbrk,
    [SYS_sleep] sys_sleep,
    [SYS_uptime] sys_uptime,
    [SYS_open] sys_open,
    [SYS_write] sys_write,
    [SYS_mknod] sys_mknod,
    [SYS_unlink] sys_unlink,
    [SYS_link] sys_link,
    [SYS_mkdir] sys_mkdir,
    [SYS_close] sys_close,
};

/*
 *  在 riscv64 中，系统调用是通过寄存器传参，具体而言
 *  依次从 a0,a1...,a6
 *  a7 存放系统调用号
 *  中断发生的时候，这些寄存器会被保存在线程的 trapframe 中。
 */

void syscall()
{
    struct thread_info *p = myproc();
    int n = p->tf->a7;
    p->tf->a0 = (uint64)syscalls[n]();
    printk("p->tf->a0: %p\n",p->tf->a0);
}