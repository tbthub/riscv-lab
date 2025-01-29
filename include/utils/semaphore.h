#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "utils/fifo.h"
#include "utils/spinlock.h"
#include "utils/atomic.h"

typedef struct semaphore
{
    atomic_t value;      // 信号量的值
    spinlock_t lock;     // 信号量的自旋锁
    struct fifo waiters; // 等待队列
} semaphore_t;

extern void sem_init(semaphore_t *sem, int value, const char *name);
extern void sem_wait(semaphore_t *sem);
extern void sem_signal(semaphore_t *sem);

#endif