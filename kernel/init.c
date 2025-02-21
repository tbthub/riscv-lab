#include "dev/devs.h"
#include "dev/blk/buf.h"
#include "core/work.h"
#include "../test/t_head.h"
#include "dev/devs.h"
#include "../user/user.h"
#include "core/sched.h"

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
    efs_mount(&virtio_disk);
    
    // 初始化第一个用户线程
    user_init();
    // user_init2();
    // user_init2();
    // user_init2();
}

void init_s()
{
    kthread_create(init_thread,NULL,"init_t",NO_CPU_AFF);
}