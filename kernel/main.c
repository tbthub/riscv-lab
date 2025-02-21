// start() jumps here in supervisor mode on all CPUs.
#include "defs.h"

#include "std/stdio.h"
#include "std/stddef.h"

#include "mm/slab.h"
#include "mm/mm.h"
#include "mm/kmalloc.h"

#include "lib/math.h"
#include "lib/fifo.h"
#include "lib/atomic.h"
#include "lib/string.h"
#include "lib/semaphore.h"

#include "core/vm.h"
#include "core/trap.h"
#include "core/proc.h"
#include "core/sched.h"

#include "dev/plic.h"
#include "dev/uart.h"
#include "dev/cons.h"
#include "dev/blk/blk_dev.h"

#include "../test/t_head.h"
volatile static int started = 0;
extern void init_s();

void main()
{
     if (cpuid() == 0)
     {

          cons_init();
          mm_init();
          kvm_init();
          trap_init();
          plic_init();
          
          proc_init();
          sched_init();

          kvm_init_hart();
          trap_inithart();
          plic_inithart();
          
          init_s();

          __sync_synchronize();
          started = 1;
     }
     else
     {
          while (started == 0)
               ;
          __sync_synchronize();
          kvm_init_hart();
          trap_inithart();
          plic_inithart();
     }
     __sync_synchronize();
     printk("hart %d starting\n", cpuid());

     scheduler();
}
