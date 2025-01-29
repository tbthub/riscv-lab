#ifndef __TEST_H__
#define __TEST_H__

void mm_test();         // 内存管理（伙伴）
void kmem_cache_test(); // slab

void thread_test();     // 内核线程
void sem_test();        // 信号量
void mutex_test();      // 互斥量
void sleep_test();      // 睡眠锁

void hash_test();       // 哈希表

void buf_test();        // 缓冲区

void block_func_test();  // 块设备测试

void work_queue_test(); // 工作队列

void timer_test();  // 测试定时器

void easy_fs_test();    // easy-fs

int bitmap_test();  // 位图
#endif