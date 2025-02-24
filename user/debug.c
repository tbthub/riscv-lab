#include "user.h"
int main()
{
   int a0 = 0;
   const char *a1 = "This is a1 args";
   const void *a2 = (const void *)0x200000000;
   char *a3[3] = {"hello", "world", 0};
   uint64 a4 = 4;
   int a5 = -1;

   for (;;)
   {
      debug(a0, a1, a2, a3, a4, a5);
   }
}