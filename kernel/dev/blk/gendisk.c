#include "dev/blk/gendisk.h"
#include "dev/devs.h"
#include "dev/blk/buf.h"
#include "dev/blk/bio.h"
#include "dev/blk/flush.h"
#include "vm.h"
#include "mm/mm.h"
#include "core/timer.h"
#define BLK_SIZE 4096

// 注意，必须在进程上下文进行测试
static int gen_open(struct gendisk *gd, mode_t mode);
static int gen_release(struct gendisk *gd, mode_t mode);
static __attribute__((noreturn)) int gen_start_io(struct gendisk *gd);

static int gen_ll_rw(struct gendisk *gd, struct bio *bio, uint32 rw);
static uint64 gen_disk_size(struct gendisk *gd);

static int gen_read(struct gendisk *gd, uint32 blockno, uint32 offset, uint32 len, void *vaddr);
static int gen_write(struct gendisk *gd, uint32 blockno, uint32 offset, uint32 len, void *vaddr);

static __attribute__((noreturn)) void kthread_gen_start_io(void *args)
{
    struct gendisk *gd = (struct gendisk *)args;
    // 反复进行磁盘 IO 操作，注意 start_io 不应该返回
    for (;;)
        gd->ops.start_io(gd);
}

// 初始化通用块
void gendisk_init(struct block_device *bd, const struct gendisk_operations *ops)
{
    struct gendisk *gd = &bd->gd;

    gd->dev = bd;
    rq_queue_init(gd);

    struct gendisk_operations *gd_ops = &gd->ops;
    gd_ops->open = (ops->open) ? (ops->open) : gen_open;
    gd_ops->release = (ops->release) ? (ops->release) : gen_release;
    gd_ops->start_io = (ops->start_io) ? (ops->start_io) : gen_start_io;
    gd_ops->disk_size = (ops->disk_size) ? (ops->disk_size) : gen_disk_size;
    gd_ops->ll_rw = (ops->ll_rw) ? (ops->ll_rw) : gen_ll_rw;
    gd_ops->read = (ops->read) ? (ops->read) : gen_read;
    gd_ops->write = (ops->write) ? (ops->write) : gen_write;

    bhash_init(&gd->bhash, gd);
    kthread_create(kthread_gen_start_io, gd, "gen_start_io",NO_CPU_AFF);

    kthread_create(flush_bhash, &gd->bhash, "gen_flush_bhash",NO_CPU_AFF);
}

// 这个重要
static __attribute__((noreturn)) int gen_start_io(struct gendisk *gd)
{
    struct request *rq;
    struct bio *bio, *tmp;
    struct buf_head *buf;

    for (;;)
    {
        // 这里要加信号量进行同步控制，有请求操作才运行，否则陷入睡眠, read\write singal
        // 每次等待一个请求

        sem_wait(&gd->queue.sem);
        rq = get_next_rq(&gd->queue);
        // 处理这个请求的 bio 链
        bio = rq->bio;
        while (bio)
        {
            if (bio->len == 0)
                continue;
            buf = buf_get(gd, bio->b_blockno);
            buf_pin(buf);
            switch (rq->rq_flags)
            {
            case DEV_READ: // 如果是读设备

                // 如果缓存中原来不存在这个块,则更新数据，如果已经存在，则直接读
                if (buf_is_new(buf))
                {
#ifdef DEBUG_GEN_BUF
                    printk("r  bno :%d, off: %d, len: %d, \tbuf miss, read start\n", buf->blockno, bio->offset, bio->len);
#endif
                    bio->b_page = buf->page;
                    gd->ops.ll_rw(gd, bio, rq->rq_flags);
#ifdef DEBUG_GEN_BUF
                    printk("r  bno :%d, off: %d, len: %d, \tread ok\n", buf->blockno, bio->offset, bio->len);
#endif
                }
#ifdef DEBUG_GEN_BUF
                else
                    printk("r  bno :%d, off: %d, len: %d, \tbuf hit\n", buf->blockno, bio->offset, bio->len);
#endif
                copy_to_user(rq->vaddr, buf->page + bio->offset, bio->len);

                buf_release(buf, 0);
                buf_unpin(buf);
                break;

            case DEV_WRITE: // 如果是写设备  把数据从用户区域复制到缓存
                bio->b_page = buf->page;
                // 部分写 且 缓存原来 不存在，则需要先读进来，即 全覆盖写 或者 缓存存在 时不必重读
                if (!(bio->offset == 0 && bio->len == BLK_SIZE) && buf_is_new(buf))
                {
#ifdef DEBUG_GEN_BUF
                    printk("w  bno :%d, off: %d, len: %d, \tpartial write or not exist, read it!\n", buf->blockno, bio->offset, bio->len);
#endif
                    gd->ops.ll_rw(gd, bio, DEV_READ);
#ifdef DEBUG_GEN_BUF
                    printk("w  bno :%d, off: %d, len: %d, \tread ok\n", buf->blockno, bio->offset, bio->len);
#endif
                }
#ifdef DEBUG_GEN_BUF
                else if (!(bio->offset == 0 && bio->len == BLK_SIZE) && !buf_is_new(buf))
                {
                    printk("w  bno :%d, off: %d, len: %d, \tpartial write | buf exists.\n", buf->blockno, bio->offset, bio->len);
                }
                else if ((bio->offset == 0 && bio->len == BLK_SIZE) && buf_is_new(buf))
                    printk("w  bno :%d, off: %d, len: %d, \tfull write | buf new \n", buf->blockno, bio->offset, bio->len);
                else
                    printk("w  bno :%d, off: %d, len: %d, \tfull write | exists \n", buf->blockno, bio->offset, bio->len);
#endif
                copy_from_user(buf->page + bio->offset, rq->vaddr, bio->len);
                buf_release(buf, 1);
                buf_unpin(buf);

                // 嗯哼，就没了。。。。并没有真正写回块设备的欧
                break;
            default:
                printk("Unknown gendisk operation\n");
                break;
            }
            tmp = bio;
            rq->vaddr += bio->len;
            bio = bio->b_next;
            bio_del(tmp);
        }
        // 唤醒在 这个 rq 上睡眠的线程
        wake_up(&rq->lock);
        // 继续去处理下一个
    }
}

static int gen_open(struct gendisk *gd, mode_t mode)
{
    printk("gen_open\n");
    return 0;
}

static int gen_release(struct gendisk *gd, mode_t mode)
{
    printk("gen_release\n");
    return 0;
}

static uint64 gen_disk_size(struct gendisk *gd)
{
    printk("gen_disk_size\n");
    return gd->dev->disk_size;
}

static int gen_ll_rw(struct gendisk *gd, struct bio *bio, uint32 rw)
{
    printk("gen_ll_rw\n");
    return 0;
}

static int gen_read(struct gendisk *gd, uint32 blockno, uint32 offset, uint32 len, void *vaddr)
{
    struct request *rq = make_request(gd, blockno, offset, len, vaddr, DEV_READ);

    // 唤醒磁盘 IO 线程执行这个 request
    sem_signal(&gd->queue.sem);

    // 在这个 rq 上睡眠，直到这个请求完成
    sleep_on(&rq->lock);

    // 这个 rq 被处理完成，释放资源
    rq_del(rq);

    return 0;
}

static int gen_write(struct gendisk *gd, uint32 blockno, uint32 offset, uint32 len, void *vaddr)
{
    struct request *rq = make_request(gd, blockno, offset, len, vaddr, DEV_WRITE);

    // 唤醒磁盘 IO 线程执行这个 request
    sem_signal(&gd->queue.sem);

    // 在这个 rq 上睡眠，直到这个请求完成
    sleep_on(&rq->lock);

    // 这个 rq 被处理完成，释放资源
    rq_del(rq);

    return 0;
}

// 读设备
inline int gen_disk_read(struct gendisk *gd, uint32 blockno, uint32 offset, uint32 len, void *vaddr)
{
    if (!vaddr)
    {
        printk("gendisk.c - gen_disk_read -  %s The given vaddr pointer is null \n", gd->dev->name);
        return -1;
    }
    return gd->ops.read(gd, blockno, offset, len, vaddr);
}

// 写设备
inline int gen_disk_write(struct gendisk *gd, uint32 blockno, uint32 offset, uint32 len, void *vaddr)
{
    if (!vaddr)
    {
        printk("gendisk.c - gen_disk_write -  %s The given vaddr pointer is null \n", gd->dev->name);
        return -1;
    }
    return gd->ops.write(gd, blockno, offset, len, vaddr);
}
