#ifndef __CONF_H__
#define __CONF_H__

#define DEBUGS
#ifdef DEBUGS

// #define DEBUG_TEST         // 用于测试程序
// #define DEBUG_ORD          // 普通调试信息
// #define DEBUG_FS           // 初始显示文件系统信息
// #define DEBUG_FLUSH        // 显示 flush 线程的刷写信息
// #define DEBUG_EFS_SYNC     // 显示 efs_sync 同步 sb,imap,bmap,inode的信息
// #define DEBUG_BIO          // 显示 bio 请求链
// #define DEBUG_GEN_BUF      // 显示 gendisk buf 读写情况
// #define DEBUG_TASK_ADD_CPU // 显示任务添加到所在的 cpuid
#define DEBUG_TASK_ON_CPU  // 显示当前 CPU 上运行的线程名
// #define DEBUG_RQ           // 显示 IO 请求

#endif

#define MAX_PATH_LEN 256 // 支持最长路径长度
#define ARG_MAX 128 * 1024 // 传入参数最长 128K
#define ENV_MAX 128 * 1024 // 环境变量最长 128K


#define USER_TEXT_BASE 0x200000000  // 用户程序代码段基地址 0x200_000_000
#define USER_STACK_TOP (0x200000000 - 0x1000 -0x8)  // 用户程序栈顶




#endif