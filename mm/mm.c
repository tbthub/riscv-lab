#include "utils/spinlock.h"
#include "utils/list.h"
#include "mm/buddy.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "std/stddef.h"
#include "riscv.h"
#include "mm/memlayout.h"
#include "std/stdio.h"
#include "mm/kmalloc.h"
#include "utils/string.h"
#include "defs.h"
// 注：我们的伙伴算法，实际上只是管理 mem_map.pages 这个数组
// 并不涉及具体页面地址的管理
// 由于这个数组元素与页面地址一一对应的关系
// 我们仅在分配/释放页面的出入口有所处理

struct buddy_struct buddy;

// first address after kernel. defined by kernel.ld.
// 由编译器最后计算出来，位于代码段和数据段的顶端
// extern uint32 kernel_pfn_end;

static struct page *find_buddy(const struct page *page, const int order)
{
    uint64 index = page - mem_map.pages;
    uint64 buddy_index = index ^ (1 << order);
    if (buddy_index >= ALL_PFN)
    {
        // 如果伙伴超出了内存页的范围，返回 NULL
        printk("OOM ERROR!\n");
        return NULL;
    }
    return mem_map.pages + buddy_index;
}

static struct page *buddy_alloc(const int order)
{
    if (order < 0 || order > MAX_LEVEL_INDEX)
        return NULL;

    int i, j;
    struct page *page;
    struct page *buddy_page;

    spin_lock(&buddy.lock);
    for (i = order; i < MAX_LEVEL; i++)
    {
        // 找到有一个不为空的
        if (!list_empty(&buddy.free_lists[i]))
        {
            // 弹出一个 page
            page = list_entry(list_pop(&buddy.free_lists[i]), struct page, buddy);

            // 拆分块直到满足所需 order
            for (j = i; j > order; j--)
            {
                // 寻找下一级的伙伴(这里的伙伴存在的话一定是空闲的)
                buddy_page = find_buddy(page, j - 1);
                if (!buddy_page)
                    break;

                // 加到下一级的链表中
                list_add_head(&buddy_page->buddy, &buddy.free_lists[j - 1]);
            }
            // 循环接受，该 page 也就是这个被拆分大块的起始地址（现在变成小块了）
            spin_unlock(&buddy.lock);
            return page;
        }
    }
    // 没有找到
    spin_unlock(&buddy.lock);
    return NULL;
}

static void buddy_free(struct page *pg, const int order)
{
    int level;
    struct page *page = pg, *buddy_page;

    spin_lock(&buddy.lock);

    // 向上合并伙伴
    for (level = order; level < MAX_LEVEL_INDEX; level++)
    {
        buddy_page = find_buddy(page, level);
        if (list_empty(&buddy.free_lists[level]))
            break;
        // 伙伴不在，也就不用向上合并了
        // 如果页面不空闲，则已经被分配出去了，现在一定不在伙伴系统里面
        if (buddy_page == NULL || !is_page_free(buddy_page))
            break;

        // 如果有伙伴块的话
        list_del_init(&buddy_page->buddy);
        list_del_init(&page->buddy);

        // page 始终为位置更低的，这样最后的 page 就是最后大块的头儿
        page = page < buddy_page ? page : buddy_page;
    }
    list_add_head(&page->buddy, &buddy.free_lists[level]);
    spin_unlock(&buddy.lock);
}

// 初始化 Buddy 系统
static void buddy_init()
{
    uint64 i;

    // 初始化锁
    spin_init(&buddy.lock, "buddy");

    // 初始化伙伴的每个链表节点
    for (i = 0; i < MAX_LEVEL; i++)
        INIT_LIST_HEAD(&buddy.free_lists[i]);

    for (i = kernel_pfn_end + 1; i < ALL_PFN; i += MAX_LEVEL_COUNT)
        list_add_head(&mem_map.pages[i].buddy, &buddy.free_lists[MAX_LEVEL_INDEX]);

    // 处理最后一点不足 max_level_count 的
    list_pop(&buddy.free_lists[MAX_LEVEL_INDEX]);

    uint end_free_count = (ALL_PFN - kernel_pfn_end) % MAX_LEVEL_COUNT;
    // 假装分配
    for (i = ALL_PFN - end_free_count; i < ALL_PFN; i++)
        page_push(&mem_map.pages[i]);

    __sync_synchronize();
    // 一个一个回收
    for (i = ALL_PFN - end_free_count; i < ALL_PFN; i++)
        free_page(mem_map.pages + i);

    // printk("Buddy system: %d blocks\n",ALL_PFN - kernel_pfn_end);
}

// 内存管理初始化: page、buddy、kmem_cache、kmalloc
void mm_init()
{
    first_all_page_init();
    buddy_init();
    kmem_cache_init();
    kmalloc_init();
    printk("mm_init ok\n");
}

// 分配 pages
struct page *alloc_pages(uint32 flags, const int order)
{
    struct page *pages = buddy_alloc(order);
    for (int i = 0; i < (1 << order); i++)
        page_push(pages + i);

    return pages;
}

void *__alloc_pages(uint32 flags, const int order)
{
    return (void *)get_page_addr(alloc_pages(flags, order));
}

// 分配一个 page
struct page *alloc_page(uint32 flags)
{
    struct page *page = buddy_alloc(0);
    page_push(page);

    return page;
}

void *__alloc_page(uint32 flags)
{
    void *addr = (void *)get_page_addr(alloc_page(flags));
    memset(addr, 0, PGSIZE);
    return addr;
}

// 释放 pages
void free_pages(struct page *pages, const int order)
{
    for (int i = 0; i < (1 << order); i++)
        page_pop(pages + i);

    buddy_free(pages, order);
}

void __free_pages(void *addr, const int order)
{
    free_pages(get_page_struct((uint64)addr), order);
}

// 释放一个 page
void free_page(struct page *page)
{
    page_pop(page);
    buddy_free(page, 0);
}

void __free_page(void *addr)
{
    free_page(get_page_struct((uint64)addr));
}



