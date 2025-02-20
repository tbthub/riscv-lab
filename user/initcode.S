# Initial process that execs /init.
# This code runs in user space.

# include "core/syscall.h"

_start:
        li a7,SYS_debug
        ecall
        j  _start
