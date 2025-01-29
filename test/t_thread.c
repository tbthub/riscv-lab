#include "core/proc.h"
#include "std/stdio.h"
#include "utils/semaphore.h"

static void testB_fun()
{
     int a = 0;
     while (a < 20000000)
     {
          if (++a % 4000000 == 0)
               printk("B-pid: %d,cpu: %d\n", myproc()->pid, cpuid());
     }
}
static void testC_fun()
{
     int a = 0;
     while (a < 40000000)
     {
          if (++a % 4000000 == 0)
               printk("C-pid: %d,cpu: %d\n", myproc()->pid, cpuid());
     }
}

static void testD_fun()
{
     printk("D, cpu: %d\n", cpuid());
}

static void testA_fun()
{
     int a = 0;
     volatile int BCD = 0;
     while (a < 50000000 + 60000000)
     {
          // 在某一时间某加入BCD
          if (++a % 4000000 == 0)
               printk("A-pid: %d,cpu: %d\n", myproc()->pid, cpuid());
          if (a > 25000000 && BCD == 0)
          {
               BCD = 1;
               thread_create(testB_fun, NULL, "testB");
               thread_create(testC_fun, NULL, "testC");
               thread_create(testD_fun, NULL, "testD");
          }
     }
}

void thread_test()
{
     if (cpuid() == 0)
     {
          thread_create(testA_fun, NULL, "testA");
     }
}