#ifndef __SCHED_H__
#define __SCHED_H__

#include "utils/spinlock.h"
#include "utils/list.h"

struct sched_struct
{
    spinlock_t lock;
    struct list_head run;
    struct list_head out;
};


void scheduler();
void yield();
void sched(void);
void quit();

struct thread_info *pick_next_task(struct list_head *run_list);
void wakeup_process(struct thread_info *thread);
#endif