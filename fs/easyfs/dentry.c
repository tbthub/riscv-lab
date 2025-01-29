// #include "easyfs.h"
// #include "utils/string.h"
// #include "dev/blk/blk_dev.h"

// struct easy_dentry root_dentry;

// // void efs_fill_dentry(struct easy_dentry *dentry, struct easy_dentry *parent, const struct easy_dirent *dirent)
// // {
// //     dentry->d_inode_no = dirent->inode_no;
// //     dentry->d_type = dirent->type;
// //     strdup(&dentry->d_name, dirent->name);
// //
// //     // dentry->d_inode =
// //     dentry->d_parent = parent;
// //     INIT_LIST_HEAD(&dentry->d_child);
// //
// //     INIT_HASH_NODE(&dentry->d_hnode);
// //     INIT_LIST_HEAD(&dentry->d_list);
// //     INIT_LIST_HEAD(&dentry->d_dirty);
// //
// //     // 在这里解析出子目录链表，链接填充上去
// // }

// inline int efs_hashd(const struct easy_dentry *dentry)
// {
//     return strhash(dentry->d_name);
// }


// void efs_rootd_obtain()
// {
//     root_dentry.d_inode_no = 1;
//     root_dentry.d_type = F_DIR;
//     strdup(&root_dentry.d_name, "/");

//     root_dentry.d_inode = &root_m_inode;
//     root_dentry.d_parent = NULL;
//     INIT_LIST_HEAD(&root_dentry.d_child);

//     INIT_HASH_NODE(&root_dentry.d_hnode);
//     INIT_LIST_HEAD(&root_dentry.d_list);
//     INIT_LIST_HEAD(&root_dentry.d_dirty);
// }


// void efs_infod(const struct easy_dentry *dentry)
// {
//     printk("easy-fs dentry infomation:\n");
//     printk("  inode_no: %d\n", dentry->d_inode_no);
//     printk("  type: %d\n", dentry->d_type);
//     printk("  name: %s\n", dentry->d_name);
//     printk("  inode: %p\n", dentry->d_inode);
//     printk("  parent: %p\n", dentry->d_parent);
// }