#include "mm/page.h"
#include "mm/memlayout.h"
#include "lib/atomic.h"
#include "defs.h"
#include "lib/list.h"
#include "riscv.h"
#include "std/stdio.h"
#include "lib/spinlock.h"
#include "defs.h"

// 需要持有 mem_map 锁
struct mem_map_struct mem_map;

// first address after kernel. defined by kernel.ld.
// 由编译器最后计算出来，位于代码段和数据段的顶端
uint32 kernel_pfn_end;

inline struct page* get_page_struct(uint64 page)
{
    return mem_map.pages + page_to_index(page);
}

inline uint64 get_page_addr(struct page *pg)
{
    return index_to_page(pg - mem_map.pages);
}

// 检查一个块是否为空闲的伙伴块
int is_page_free(struct page *page)
{
    return page_count(page) == PG_FREE;
}

void page_init(struct page *pg,uint32 flags)
{
    pg->flags = flags;
    atomic_set(&pg->count,PG_FREE);
    INIT_LIST_HEAD(&pg->buddy);
    pg->slab = NULL;
}

// 首次初始化
void first_all_page_init()
{
    int i;
    // 内核代码数据空间
    kernel_pfn_end = END_PFN - KERNBASE_PFN;
    spin_init(&mem_map.lock,"mem_map");
    // 省去条件判断写成两个循环，额，会快一点？
    for (i = 0; i < ALL_PFN; i++)
        page_init(&mem_map.pages[i],0);
    
    for (i = 0; i < kernel_pfn_end; i++)
    {
        atomic_inc(&mem_map.pages[i].count);
        SetPageFlag(&mem_map.pages[i], PG_reserved | PG_anon);
    }
}


inline int page_count(struct page *pg)
{
    return atomic_read(&pg->count);
}

// 引用 +1
inline void page_push(struct page *page)
{
    atomic_inc(&page->count);
}

// 引用 -1
inline void page_pop(struct page *page)
{
    atomic_dec(&page->count);
}