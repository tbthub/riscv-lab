#ifndef __VM_H__
#define __VM_H__
#include "std/stddef.h"
extern void kvm_init();
extern void kvm_init_hart();

extern void copy_to_user(void *to, const void *from, uint64 len);
extern void copy_from_user(void *to, const void *from, uint64 len);

#endif