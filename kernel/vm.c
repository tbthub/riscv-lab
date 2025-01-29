#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[]; // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t)kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // 物理地址恒等映射
  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  // 内核代码段的映射（可读可执行）
  // etext 表示内核代码的结束位置。链接器在内核编译时会生成这个符号，表示内核代码段的末尾地址。参见kernel.ld
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  // 内核数据段和物理内存的映射（可读可写，包括堆、栈、BSS）
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  // 映射 TRAMPOLINE 页面(虚存的顶端)
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  // 为每一个进程分配并映射到内核栈，在 kenrel vm 的顶端分配 64 个进程的内核栈
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

// Initialize the one kernel_pagetable
void kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// 将硬件页表寄存器 satp 切换到内核的页表（stap寄存器保存页表地址），
// 并启用分页功能。
void kvminithart()
{
  // 等待之前对页表内存的任何写操作完成。
  sfence_vma();

  // 将页表切换到内核的页表。
  // （此后就开始虚拟地址了,在kvm中，内核页表是恒等映射，内核的虚拟地址与物理地址一样，所以代码继续往下执行，没问题）
  w_satp(MAKE_SATP(kernel_pagetable));

  // 刷新TLB（翻译后备缓冲区）中的过期条目。
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--)
  {
    pte_t *pte = &pagetable[PX(level, va)];

    // 如果该页表项有效（Valid 位）
    // 额外地，V 位有效则一定是有映射实际的物理页面的，
    // 只不过该实际页面的内容在不在内存中就是 swap 的事情了
    if (*pte & PTE_V)
    {
      pagetable = (pagetable_t)PTE2PA(*pte);
    }

    // 无效，这里看需不需要分配，即 alloc 的值
    else
    {
      // 如果不需要分配、或者已经没有页面可以分配了
      if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
        return 0;
      // 需要分配的话上面 if 判断中已申请到一个页面
      // 注：申请到的页面地址是真实的物理地址
      memset(pagetable, 0, PGSIZE);
      // 设置页表项的属性
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
// 只能寻找用户页面，也就是必须是 PTE_U 为 1 的页面，返回对应的真实的地址
// 为什么我们不需要寻找内核的，大部分是恒等的。
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
// 内核虚存映射:将一个虚拟地址（va）映射到指定的物理地址（pa），大小为 sz，并设置相应的权限（perm）
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
// 从 va 到 va + sz 的虚拟地址范围映射到从 pa 到 pa + sz 的物理地址范围。
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
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
  last = va + size - PGSIZE;
  for (;;)
  {
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;

    if (*pte & PTE_V)
      panic("mappages: remap");

    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 从 va 开始移除 npages 的映射。va 必须是页对齐的。
// 映射必须存在
// 可选地释放物理内存。
// 修改这 npages 个物理页面的头上那个页表项，直接清 0 即可，
// 如果是普通进程，或者说私有的映射，在解映射的时候要记得释放掉物理页面 do_free 为1。
// 如果是共享页面映射的话，请不要释放，比如，你也不想 trampoline 被释放掉吧。
// 这里的节点就是叶子映射，我们在这里进行清除
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if ((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for (a = va; a < va + npages * PGSIZE; a += PGSIZE)
  {
    if ((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");

    if ((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");

    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");

    if (do_free)
    {
      uint64 pa = PTE2PA(*pte);
      kfree((void *)pa);
    }
    // 把该页表项清空，即解除映射
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 将 user initcode 加载到页表的地址 0 (虚拟地址空间中的地址 0)
// 这是第一个进程的初始化代码
// sz 必须小于一页
void uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if (sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  // 把这个 mem 映射到虚拟地址的起始位置
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_X | PTE_U);

  // 复制代码过去（注：这里是在内核中复制的，恒等映射，直接复制到这个 mem 页面）
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
// 只能用于页面级别的增长分配
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE)
  {
    mem = kalloc();
    if (mem == 0)
    {
      // 失败就回滚
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }

    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm) != 0)
    {
      kfree(mem);
      // 失败就回滚
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.

// 用户页面减少、取消映射并释放
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
  {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// 递归地释放页表页面。
// 所有叶子映射必须已经被移除。
void freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
    {
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    }
    // 结合上面那个 if 到达这里应该是页表项有效，但是有相应的读写执行权限
    // 表示这是一个指向物理页面的页表项，也就是叶子映射
    else if (pte & PTE_V)
    {
      panic("freewalk: leaf");
    }
  }
  kfree((void *)pagetable);
}

// Free user memory pages,
// then free page-table pages.
// 释放所有用户虚拟内存空间并释放页面
void uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);   // 这里把叶子释放掉了
  freewalk(pagetable);
}

// 给定父进程的页表，将其内存复制到子进程的页表中。 
// 复制页表和物理内存。（俺的写时复制呢，呜哇） 
// 成功时返回 0，失败时返回 -1。 
// 失败时会释放任何已分配的页面。
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE)
  {
    // 从 old 的虚拟 0 地址开始依次获取每一个页表项（走的是 old 的页表）
    if ((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");

    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    
    if ((mem = kalloc()) == 0)
      goto err;
    
    // 这里是在复制页数据？
    memmove(mem, (char *)pa, PGSIZE);

    // 把这个页映射到新的页表中
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0)
    {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE __invalid for user access__ .
// used by exec for the user stack guard page.
void uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
// 复制src到用户的 dstva 开始的虚拟地址空间（内核页表是全局变量，且大部分恒等映射的）
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while (len > 0)
  {
    va0 = PGROUNDDOWN(dstva);
    if (va0 >= MAXVA)
      return -1;

    pte = walk(pagetable, va0, 0);
    
    // 如果 pte 条目不存在、pte 本身是否有效、用户不可访问、不可写
    if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_W) == 0)
      return -1;
    
    pa0 = PTE2PA(*pte);
    n = PGSIZE - (dstva - va0);   // n：当前物理页面在 dstva 后剩余的空间
    
    // 如果剩余的空间足够容纳这 len 个字节
    if (n > len)
      n = len;
    
    // 复制（注：pa0 是真实的物理地址（虽说内核是恒等映射），加上偏移）
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    // 如果len的长度大于剩余可用页面空间，则进入下一个页面
    // 下一次循环的时候 va0 = PGROUNDDOWN(dstva) = dstva，此时页面偏移为0，也就是描述整页
    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
// 同上
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while (len > 0)
  {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);

    if (pa0 == 0)
      return -1;
    
    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;
    
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// 将一个以 null 结尾的字符串从用户空间复制到内核空间。
// 从给定页表中的虚拟地址 srcva 开始，直到遇到 '\0' 或达到最大字节数，将字节复制到 dst。
// 成功时返回 0，失败时返回 -1。
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while (got_null == 0 && max > 0)
  {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    char *p = (char *)(pa0 + (srcva - va0));
    while (n > 0)
    {
      if (*p == '\0')
      {
        *dst = '\0';
        got_null = 1;
        break;
      }
      else
      {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if (got_null)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}
