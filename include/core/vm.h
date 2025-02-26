#ifndef __VM_H__
#define __VM_H__
#include "std/stddef.h"


struct vm_operations_struct;
struct thread_info;

#define PROT_NONE (1L << 0)
#define PROT_EXEC (1L << 1)
#define PROT_WRITE (1L << 2)
#define PROT_READ (1L << 3)
#define PROT_LAZY (1L << 31)

// Virtual Memory Area
struct vm_area_struct
{
    uint64 vm_start;      // 区域起始地址
    uint64 vm_end;        // 区域结束地址
    flags_t vm_prot;       // 区域标志 RWX
    uint32 vm_pgoff;      // 文件页偏移
    struct file *vm_file; // 关联文件
    struct vm_area_struct *vm_next;
    struct vm_operations_struct *vm_ops;
};

// https://don7hao.github.io/2015/01/28/kernel/mm_struct/
struct mm_struct
{
    pagetable_t pgd;

    uint64 start_code;
    uint64 end_code;

    uint64 start_data;
    uint64 end_data;

    uint64 start_brk;
    uint64 end_brk;

    uint64 start_stack;

    struct vm_area_struct *mmap;
    spinlock_t lock; // 保护并发访问
    int map_count;   // VMA 数量

    uint64 size; // 总内存使用量
};

struct vm_operations_struct
{
    void (*fault)(struct mm_struct *, struct vm_area_struct *, uint64); // 缺页中断
};

extern void kvm_init();
extern void kvm_init_hart();
extern pagetable_t alloc_pgt();

extern void uvmfirst(struct thread_info *init, uchar *src, uint sz);

#endif