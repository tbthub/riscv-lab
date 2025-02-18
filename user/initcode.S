# Initial process that execs /init.
# This code runs in user space.

#include "core/syscall.h"

# exec(init, argv)
        .globl user_start
user_start:
        li     a0, 0
        li     a1, 0
        ecall