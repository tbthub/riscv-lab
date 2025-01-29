#ifndef __TRAP_H__
#define __TRAP_H__

#define TIMER_SCAUSE 0x8000000000000005L
#define EXTERNAL_SCAUSE 0x8000000000000009L

void trap_init();
void trap_inithart();

#endif