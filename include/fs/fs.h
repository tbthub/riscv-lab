// #ifndef __FS_H__
// #define __FS_H__
// #include "std/stddef.h"
// #include "utils/list.h"
// #include "dev/blk/blk_dev.h"
// #include "utils/spinlock.h"

// struct file_system_type
// {
//     char name[16];                                                      // 文件系统类型名称（唯一）
//     struct dentry *(*mount)(struct file_system_type *, int mount_flags, // 挂载函数
//                             const char *blk_dev_name, void *priv);
//     void (*kill_sb)(struct super_block *); // 删去超级块实例函数
//     struct file_system_type *next;         // 指向下一个 file_system_type
//     struct list_head fs_super;             // 指向该类型从超级块链表
// };

// struct super_block
// {
//     spinlock_t lock;            // 保护超级块的自旋锁
//     struct list_head s_list;    // 超级块链表
//     struct block_device *s_dev; // 设备指针
//     uint32 s_block_size;        // 数据块大小
//     struct file_system_type *s_type;
//     uint32 flags;
//     struct super_operations *s_ops; // 超级块操作指针

//     struct list_head s_inodes;
//     struct list_head s_dentry_lru; // dentry 的 lru 链
//     struct list_head s_inode_lru;  // inode 的 lru 链
//     uint32 s_magic;
//     struct dentry *s_root;
// };

// struct inode
// {

// };

// struct dentry
// {

// };


// struct super_operations {
//     struct inode *(*alloc_inode)(struct super_block *sb);
//     struct inode *(*destory_inode)(struct inode *);
//     // 标记 inode 节点脏
//     struct inode *(*dirty_inode)(struct inode *,int flags);
//     // 写回 inode 节点
//     int (*write_inode)(struct inode *);
//     // 最后一个索引节点的引用被释放后，调用该函数
//     int (*drop_inode)(struct inode *);
//     //释放超级块
//     void (*put_super)(struct super_block *sb);
//     // 文件系统元数据同步
//     int (*sync_fs)(struct super_block *sb,int wait);
// };

// #endif