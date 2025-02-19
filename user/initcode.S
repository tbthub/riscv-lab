# Initial process that execs /init.
# This code runs in user space.

#include "core/syscall.h"

_start:
        li a0, 1
        li a1, 1
        j  _start
