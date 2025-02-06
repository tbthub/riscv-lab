#include "dev/devs.h"
#include "dev/blk/buf.h"
#include "core/work.h"
#include "../test/t_head.h"
#include "dev/devs.h"
// extern void my_dev_init();
// extern void flushd_start();

// 内存虚拟块设备
extern void mvirt_blk_dev_init();
extern void virtio_disk_init();

extern struct block_device virtio_disk;
extern int efs_mount(struct block_device *bd);

// 第一个内核线程
static void init_thread(void *)
{
    devs_init();
    work_queue_init();
    virtio_disk_init();

    // block_func_test();   // 测试硬盘读写功能

    efs_mount(&virtio_disk);
    // easy_fs_test();
    int *a = NULL;
    printk("%d", *a);
    // while (1)
    //     ;
}

void init_s()
{
    thread_create(init_thread, NULL, "init_t");
    // 测试设备
    // my_dev_init();
    // mvirt_blk_dev_init();

    // block_func_test();
    // flushd_start();
    // block_func_test();

    // work_queue_test();
    // timer_test();

    // bitmap_test();
}