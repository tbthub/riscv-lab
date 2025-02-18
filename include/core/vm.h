#ifndef __VM_H__
#define __VM_H__
#include "std/stddef.h"
#include "core/proc.h"
extern void kvm_init();
extern void kvm_init_hart();
extern pagetable_t alloc_pt();

extern void *copy_to_user(void *to, const void *from, uint64 len);
extern void *copy_from_user(void *to, const void *from, uint64 len);

extern void uvmfirst(struct thread_info *init, uchar *src, uint sz);

#endif