#
# low-level code to handle traps from user space into
# the kernel, and returns from kernel to user.
#
# the kernel maps the page holding this code
# at the same virtual address (TRAMPOLINE)
# in user and kernel space so that it continues
# to work when it switches page tables.
# kernel.ld causes this code to start at
# a page boundary.
#
# * 参考 info.md/用户中断

#include "riscv.h"
#include "mm/memlayout.h"

        .section uservec_space
        .align   4
        .globl   usertrap
        .globl   uservec
uservec:

# * sscratch 存放的是当前进程内核栈的地址，现在我们把
# * 当前的寄存器现场保存到内核栈
# * 交换 a0, sscratch 的值，此时 a0 为内核栈
        csrrw    a0, sscratch, a0

# * 把寄存器保存到内核栈中
        sd       ra, -0(a0)
        sd       sp, -8(a0)       # * 此时 sp 是用户栈顶
        sd       gp, -16(a0)
        sd       tp, -24(a0)
        sd       t0, -32(a0)
        sd       t1, -40(a0)
        sd       t2, -48(a0)
        sd       s0, -56(a0)
        sd       s1, -64(a0)
        # sd a0, -72(a0) 占位 72
        sd       a1, -80(a0)
        sd       a2, -88(a0)
        sd       a3, -96(a0)
        sd       a4, -104(a0)
        sd       a5, -112(a0)
        sd       a6, -120(a0)
        sd       a7, -128(a0)
        sd       s2, -136(a0)
        sd       s3, -144(a0)
        sd       s4, -152(a0)
        sd       s5, -160(a0)
        sd       s6, -168(a0)
        sd       s7, -176(a0)
        sd       s8, -184(a0)
        sd       s9, -192(a0)
        sd       s10, -200(a0)
        sd       s11, -208(a0)
        sd       t3, -216(a0)
        sd       t4, -224(a0)
        sd       t5, -232(a0)
        sd       t6, -240(a0)

# 初始化内核栈栈指针
        addi     sp, a0, -248

# 先保存原来 a0 的值到栈中
        csrr     t0, sscratch     # 读取原来的 a0 (即 sscratch 的值) 到 t0
        sd       t0, -72(a0)      # 保存原来的 a0 到栈中

        csrw     sscratch, a0     # 写回内核栈地址到 sscratch

# 用户程序不需要返回，采用无条件跳转
        j        usertrap




.globl   userret
userret:
# userret(pagetable)
# called by usertrapret() in trap.c to
# switch from kernel to user.


# * 读取 sscratch 的值，此时 a0 为内核栈
        csrr     a0, sscratch

# * 把寄存器保存到内核栈中
        ld       ra, -0(a0)
        ld       sp, -8(a0)       # * 此时 sp 是用户栈顶
        ld       gp, -16(a0)
        ld       tp, -24(a0)
        ld       t0, -32(a0)
        ld       t1, -40(a0)
        ld       t2, -48(a0)
        ld       s0, -56(a0)
        ld       s1, -64(a0)
# ld a0, -72(a0), 但是这一句需要最后做
        ld       a1, -80(a0)
        ld       a2, -88(a0)
        ld       a3, -96(a0)
        ld       a4, -104(a0)
        ld       a5, -112(a0)
        ld       a6, -120(a0)
        ld       a7, -128(a0)
        ld       s2, -136(a0)
        ld       s3, -144(a0)
        ld       s4, -152(a0)
        ld       s5, -160(a0)
        ld       s6, -168(a0)
        ld       s7, -176(a0)
        ld       s8, -184(a0)
        ld       s9, -192(a0)
        ld       s10, -200(a0)
        ld       s11, -208(a0)
        ld       t3, -216(a0)
        ld       t4, -224(a0)
        ld       t5, -232(a0)
        ld       t6, -240(a0)
        ld       a0,-72(a0)

# return to user mode and user pc.
# usertrapret() set up sstatus and sepc.
        sret
