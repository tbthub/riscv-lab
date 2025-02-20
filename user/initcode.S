# Initial process that execs /init.
# This code runs in user space.

# include "core/syscall.h"

_start:
        li a7,SYS_wake_test
        ecall
        j  _start
