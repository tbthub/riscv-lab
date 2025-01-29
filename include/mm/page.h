#ifndef __PAGE_H__
#define __PAGE_H__

#include "std/stddef.h"
#include "mm/memlayout.h"
#include "utils/list.h"
#include "utils/atomic.h"
#include "mm/slab.h"
#include "utils/spinlock.h"

/*
 *
 * 注意，修改页面的标志需要持有 mem_map 的锁
 *
 */

#define index_to_page(index) (uint64)(((index) << PGSHIFT) + KERNBASE)
#define page_to_index(page) (((uint64)(page) - KERNBASE) >> PGSHIFT)

#define page_to_pfn(page) ((uint64)(page) >> PGSHIFT)
#define pfn_to_page(pfn) ((uint64)(pfn) << PGSHIFT)

#define ALL_PFN ((AVAILABLE_MEMORY) >> PGSHIFT)
#define KERNBASE_PFN ((KERNBASE) >> PGSHIFT)
#define END_PFN (((uint64)end) >> PGSHIFT)

// 页面标志
#define TestPageFlag(page, flag) (((page)->flags) & (flag))
#define SetPageFlag(page, flag) (((page)->flags) |= (flag))

#define ClearPageFlag(page, flag) (((page)->flags) &= ~(flag))
#define ResetPageFlag(page) (((page)->flags) = 0)

// 页面标志位
#define PG_dirty (1 << 0)    // 页面脏
#define PG_locked (1 << 1)   // 锁定页面，不能进行页面交换或被其他进程修改（可用于写时复制）
#define PG_anon (1 << 2)     // 是否为匿名页面（非文件映射的页面），可用于堆栈、堆和内核
#define PG_reserved (1 << 3) // 是否为保留页面，避免被操作系统的分配器使用
#define PG_Slab (1 << 4) // 用于 Slab

#define PG_FREE -1

// 描述每一个物理页面
struct page
{
    uint32 flags;
    atomic_t _count; // 引用计数 -1 没有引用；0 被分配但未被显式引用
    struct list_head buddy;
    struct slab * _slab;
};

struct mem_map_struct{
    // 所有物理页面的数组，下标为页框
    // 由于我们管理 0x8000_0000 以上 128M 的页面，因此实际使用中都要加上基地址页框的偏移了
    struct page pages[ALL_PFN];
    spinlock_t lock;
};

// 需要持有 mem_map 锁
extern struct mem_map_struct mem_map;
extern uint32 kernel_pfn_end;



struct page*    get_page_struct(uint64 page);
uint64          get_page_addr(struct page *pg);
int             is_page_free(struct page *pg);

// 初始化所有页面
void            first_all_page_init();
void            page_init(struct page *pg,uint32 flags);
int             page_count(struct page *pg);
void            page_push(struct page *page);
void            page_pop(struct page *page);

#endif