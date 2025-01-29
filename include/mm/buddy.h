#ifndef __BUDDT_H__
#define __BUDDY_H__
#include "utils/spinlock.h"
#include "utils/list.h"

#define MAX_LEVEL 11
#define MAX_LEVEL_INDEX (MAX_LEVEL - 1)
#define MAX_LEVEL_COUNT (1 << MAX_LEVEL_INDEX)
struct buddy_struct
{
    spinlock_t lock;
    struct list_head free_lists[MAX_LEVEL];    
};


#endif