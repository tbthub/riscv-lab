#ifndef __FIFI_H__
#define __FIFI_H__
#include "lib/list.h"
#include "std/stddef.h"
#include "lib/list.h"

struct fifo
{
    int size;
    struct list_head list;
};

static inline void fifo_init(struct fifo *fifo)
{
    INIT_LIST_HEAD(&fifo->list);
    fifo->size = 0;
}

static inline void fifo_push(struct fifo *fifo, struct list_head *entry)
{
    list_add_tail(entry, &fifo->list);
    fifo->size++;
}

static inline int fifo_empty(struct fifo *fifo)
{
    return fifo->size == 0;
}

static inline struct list_head *fifo_pop(struct fifo *fifo)
{
    if (list_empty(&fifo->list))
        return NULL;
    fifo->size--;
    return list_pop(&fifo->list);
}

#endif