#ifndef __VM_H__
#define __VM_H__
#include "std/stddef.h"
#include "core/proc.h"
extern void kvm_init();
extern void kvm_init_hart();
extern pagetable_t alloc_pgt();

extern void uvmfirst(struct thread_info *init, uchar *src, uint sz);

#endif