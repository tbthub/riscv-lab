#ifndef __WORK_H__
#define __WORK_H__
#include "utils/fifo.h"

extern void work_queue_push(void (*func)(void *), void *args);

extern void work_queue_init();
#endif