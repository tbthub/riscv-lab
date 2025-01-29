#include "mm/slab.h"
#include "utils/math.h"
#include "mm/page.h"
#include "core/proc.h"
#include "dev/blk/buf.h"
#include "core/timer.h"
#include "dev/blk/bio.h"
#include "../fs/easyfs/easyfs.h"

struct kmem_cache kmalloc16;
struct kmem_cache kmalloc32;
struct kmem_cache kmalloc64;
struct kmem_cache kmalloc128;
struct kmem_cache kmalloc256;
struct kmem_cache kmalloc512;
struct kmem_cache kmalloc1024;
struct kmem_cache kmalloc2048;
struct kmem_cache kmalloc4096;
struct kmem_cache kmalloc8192;
struct kmem_cache *kmalloc_caches[] = {
    [0] & kmalloc16,
    [1] & kmalloc32,
    [2] & kmalloc64,
    [3] & kmalloc128,
    [4] & kmalloc256,
    [5] & kmalloc512,
    [6] & kmalloc1024,
    [7] & kmalloc2048,
    [8] & kmalloc4096,
    [9] & kmalloc8192,
};

// 专用缓存
struct kmem_cache task_struct_kmem_cache;
struct kmem_cache thread_info_kmem_cache;
struct kmem_cache buf_kmem_cache;
struct kmem_cache bio_kmem_cache;
struct kmem_cache timer_kmem_cache;
struct kmem_cache efs_inode_kmem_cache;
struct kmem_cache efs_dentry_kmem_cache;

void kmalloc_init()
{
    kmem_cache_create(&kmalloc16, "kmalloc-16", 16, 0);
    kmem_cache_create(&kmalloc32, "kmalloc-32", 32, 0);
    kmem_cache_create(&kmalloc64, "kmalloc-64", 64, 0);
    kmem_cache_create(&kmalloc128, "kmalloc-128", 128, 0);
    kmem_cache_create(&kmalloc256, "kmalloc-256", 256, 0);
    kmem_cache_create(&kmalloc512, "kmalloc-512", 512, 0);
    kmem_cache_create(&kmalloc1024, "kmalloc-1024", 1024, 0);
    kmem_cache_create(&kmalloc2048, "kmalloc-2048", 2048, 0);
    kmem_cache_create(&kmalloc4096, "kmalloc-4096", 4096, 0);
    kmem_cache_create(&kmalloc8192, "kmalloc-8192", 8192, 0);

    // 初始化专用缓存
    kmem_cache_create(&task_struct_kmem_cache, "task_struct_kmem_cache", sizeof(struct task_struct), 0);
    kmem_cache_create(&thread_info_kmem_cache, "thread_info_kmem_cache", 2 * PGSIZE, 0);
    kmem_cache_create(&buf_kmem_cache, "buf_kmem_cache", sizeof(struct buf_head), 0);
    kmem_cache_create(&bio_kmem_cache, "bio_kmem_cache", sizeof(struct bio), 0);
    kmem_cache_create(&timer_kmem_cache, "timer_kmem_cache", sizeof(struct timer), 0);
    kmem_cache_create(&efs_inode_kmem_cache, "inode_kmem_cache", sizeof(struct easy_m_inode), 0);
    kmem_cache_create(&efs_dentry_kmem_cache, "dentry_kmem_cache", sizeof(struct easy_dentry), 0);
}

void *kmalloc(int size, uint32 flags)
{
    if (size < 1 || size > 8192)
    {
        printk("!!!kmalloc: size illegal, must be between 1 and 8192!!!\n");
        return NULL;
    }

    uint32 order = calculate_order((uint32)size) - 4;
    void *addr = NULL;
    addr = kmem_cache_alloc(kmalloc_caches[order]);

    if (!addr)
    {
        printk("kmalloc: Allocation failed for size %d.\n", size);
    }

    return addr;
}

void kfree(void *obj)
{
    struct slab *slab;
    slab = get_page_struct((uint64)obj)->_slab;
    kmem_cache_free(slab->kc, obj);
}
