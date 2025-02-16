#include "riscv.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "std/stdio.h"
#include "lib/string.h"
#include "lib/spinlock.h"
#include "std/stddef.h"
#include "defs.h"

// 内核页表
pagetable_t kernel_pagetable;

//   64 位的虚拟地址字段分布
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.

// SV39 格式下的页表项分布
//  54..63  -- reserved
//  28..53  -- PPN[2]
//  19..27  -- PPN[1]
//  10..18  -- PPN[0]
//  9..8    -- RSW  (Reserved for Software)  保留,操作系统可用，比如用这个来区分普通的不可写、写时复制
//  7       -- D    (Dirty)     处理器记录自从页表项上的这一位被清零之后，页表项的对应虚拟页面是否被修改过。
//  6       -- A    (Accessed)  处理器记录自从页表项上的这一位被清零之后，页表项的对应虚拟页面是否被访问过
//  5       -- G    (Global)    指示该页表项指向的物理页面在所有进程中共享
//  4       -- U    (User)      控制索引到这个页表项的对应虚拟页面是否在 CPU 处于 U 特权级的情况下是否被允许访问；
//  3       -- X
//  2       -- W
//  1       -- R
//  0       -- V    (Valid)     仅当位 V 为 1 时，页表项才是有效的；

// 返回最下面的那个页表项的地址,并根据 alloc 的值看是否决定分配
static pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
    if (va >= MAXVA)
    {
        printk("vm.c walk: Virtual address out of bounds!");
        // (杀死当前进程?)
        return NULL;
    }

    int level;

    for (level = 2; level > 0; level--)
    {
        // 提取出虚拟地址的level级别的每9位，根据这个9位在当前页表(这个pagetable也在反复更新)中查找 pte
        pte_t *pte = &pagetable[PX(level, va)];

        // 如果该页表项有效（Valid 位）
        // 额外地，V 位有效则一定是有映射实际的物理页面的，
        // 只不过该实际页面的内容在不在内存中就是 swap 的事情了
        if (*pte & PTE_V)
        {
            pagetable = (pagetable_t)PTE2PA(*pte);
        }

        // 否则无效，这里看需不需要分配，即alloc的值
        // 由于我们采用树形结构，因此不用的 pte （没有实际映射的）可以不分配，可以节省一大块空间
        // 如果直接平铺开，则每个 pte 都要提前分配好

        else
        {
            // 如果不需要分配、或者已经没有页面可以分配了
            if (!alloc || (pagetable = (pagetable_t)__alloc_page(0)) == NULL)
                return NULL;
            // 需要分配的话上面 if 判断中已申请到一个页面
            // 注：申请到的页面地址是真实的物理地址
            memset(pagetable, 0, PGSIZE);
            // 设置页表项的属性
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    // 返回根据最后9位找到的 pte 地址
    return &pagetable[PX(0, va)];
}

// 为从 va 开始的虚拟地址创建 PTE（页表项），这些虚拟地址对应于从 pa 开始的物理地址。
// va 和 size 必须是页对齐的（size为页面的整数倍）。
// 成功时返回 0，若 walk() 无法分配所需的页表页，则返回 -1。
// 从 va 到 va + sz 的虚拟地址范围映射到从 pa 到 pa + sz 的物理地址范围。
// 再次强调下，va 区域连续，pa 区域也连续的！！！。
static int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size, int perm)
{
    uint64 a, last;
    pte_t *pte;
    if ((va % PGSIZE) != 0)
        panic("mappages: va not aligned");

    if ((size % PGSIZE) != 0)
        panic("mappages: size not aligned");

    if (size == 0)
        panic("mappages: size");

    a = va;
    // 指向最后一个映射的页面，由于个数原因，最后要减去一个页面大小。
    // 比如：0x8000_0000开始的这个页面 sz = 0x1000,则最后映射的页面就是 0x8000_0000
    last = va + size - PGSIZE;

    spin_lock(&mem_map.lock);
    for (;;)
    {
        if ((pte = walk(pagetable, a, 1)) == NULL)
            return ERR;

        if (*pte & PTE_V)
            panic("vm.c mappages: remap");
        // 查看该文件上面对SV39的字段解释
        // 添加页面映射和权限信息
        *pte = PA2PTE(pa) | perm | PTE_V;

        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    spin_unlock(&mem_map.lock);
    return 0;
}

static void kvm_map(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    if (mappages(kpgtbl, va, pa, sz, perm) != 0)
        panic("vm.c kvm_map: Error!");
}

// 内核虚存初始化
void kvm_init()
{
    pagetable_t kpgtbl;

    kpgtbl = (pagetable_t)__alloc_page(0);
    if (!kpgtbl)
        panic("Failed to apply for kernel page table space!\n");

    memset(kpgtbl, 0, PGSIZE);

    // 下面开始映射、大部分都是恒等映射

    // uart registers
    kvm_map(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // virtio mmio disk interface
    kvm_map(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // PLIC
    kvm_map(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

    // 内核代码段的映射（可读可执行）
    // etext 表示内核代码的结束位置。链接器在内核编译时会生成这个符号，表示内核代码段的末尾地址。参见kernel.ld
    kvm_map(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

    // 内核数据段的映射
    // kvm_map(kpgtbl, (uint64)etext, (uint64)etext, PGROUNDUP((uint64)end) - (uint64)etext, PTE_R | PTE_W);

    // 内核数据段和物理内存的映射（可读可写，包括堆、栈、BSS）
    kvm_map(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

    // 下面是我们暂时还没有实现的

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    // 映射 TRAMPOLINE 页面
    // kvm_map(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
    printk("kvm init ok!\n");
    kernel_pagetable = kpgtbl;
}

// 将硬件页表寄存器 satp 切换到内核的页表（stap寄存器保存页表地址），
// 并启用分页功能。
void kvm_init_hart()
{
    // 等待之前对页表内存的任何写操作完成。
    sfence_vma();

    // 将页表切换到内核的页表。
    // （此后就开始虚拟地址了,在kvm中，内核页表是恒等映射，内核的虚拟地址与物理地址一样，所以代码继续往下执行，没问题）
    w_satp(MAKE_SATP(kernel_pagetable));

    // 刷新TLB（翻译后备缓冲区）中的过期条目。
    sfence_vma();
}

// 在进程虚存管理里面，我们将内核也映射到了用户页表
// 所以大家都是在一个页表内,我们直接复制即可
void copy_to_user(void *to, const void *from, uint64 len)
{
    memcpy(to, from, len);
}

void copy_from_user(void *to, const void *from, uint64 len)
{
    memcpy(to, from, len);
}