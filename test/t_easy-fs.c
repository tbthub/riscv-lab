#include "core/proc.h"
#include "mm/mm.h"
#include "dev/blk/gendisk.h"
#include "dev/blk/blk_dev.h"
#include "core/timer.h"
#include "utils/string.h"
#include "../fs/easyfs/easyfs.h"
// 必须等到文件系统初始化完成后才可以测试
static void __easy_fs_test()
{
    // thread_timer_sleep(myproc(), 3000);
    printk("__easy_fs_test\n");

    // root
    // struct easy_m_inode *root1 = efs_i_get(1);
    // struct easy_m_inode *root2 = efs_i_get(1);
    // assert(root1 == root2 && root1 != NULL, "root1 != root2,root1: %p, root2: %p\n", root1, root2);
    // efs_i_info(root1);
    // efs_i_put(root1);
    // efs_i_put(root2);

    // new
    // struct easy_m_inode *new_i = efs_i_new();
    // struct easy_m_inode *_new_i = efs_i_get(new_i->i_di.i_no);
    // assert(new_i == _new_i && new_i != NULL, "new_i != _new_i,new_i: %p, _new_i: %p\n", new_i, _new_i);
    // efs_i_info(new_i);

    // put & ulink & dup
    // struct easy_m_inode *i1 = efs_i_get(7);
    // efs_i_type(i1, F_REG);
    // efs_i_info(i1);
    // efs_i_put(i1);
    // efs_i_unlink(i1);
    // struct easy_m_inode *i1_ = efs_i_new();
    // efs_i_info(i1_);
    // efs_i_dup(i1_);
    // efs_i_put(i1_);
    // efs_i_unlink(i1_);

    // read & write & trunc
    // struct  easy_m_inode *i2 = efs_i_new();
    // struct easy_m_inode *i2 = efs_i_new();
    // char str[] = "hello world\n";
    // efs_i_write(i2, 0, sizeof(str), str);
    // struct easy_m_inode *_new = efs_i_new();
    // int bno;
    // for (int i = 0; i < 16; i++)
    // {
    //     bno = efs_i_bmap(_new, i, 1);
    //     printk("%d -> bno: %d\n", i, bno);
    // }
    // efs_i_info(r);

    // efs_i_write(i2, 0, sizeof(str), str);
    // efs_i_trunc(i2);
    // efs_i_put(i2);
    // char *s = kmalloc(10, 0);
    // int rl = efs_i_read(i2, 6, 5, s);
    // printk("s: %s,read len:%d\n", s, rl);

    // efs_i_read(r, 2, 3, str2);
    // printk("%s\n", str2);

    // struct easy_m_inode *inode1 = efs_i_new();
    // efs_i_write(inode1, 0, 256 * BLOCK_SIZE, addr);
    // efs_i_info(inode1);
    // efs_i_put(inode1);

    // struct easy_m_inode *inode1 = efs_ialloc(1);
    // efs_iinfo(inode1);

    // struct easy_m_inode *inode2 = efs_ialloc(0);
    // efs_iinfo(inode2);

    // struct easy_m_inode *inode3 = efs_ialloc(0);
    // efs_iinfo(inode3);

    // struct easy_m_inode *inode2_ = efs_ifind(inode2->i_no);
    // if (inode2_)
    //     efs_iinfo(inode2_);
    // else
    //     printk("aaa\n");

    // efs_d_info(&root_dentry);
    // efs_d_creat(&root_dentry, "bin", F_DIR);
    // // printk("a1\n");
    // efs_d_creat(&root_dentry, "dev", F_DEV);

    // struct easy_dentry *d = efs_d_namei("/bin");
    // efs_d_info(d);
    
    // efs_d_creat(d, "hello.txt", F_REG);
    // efs_d_creat(d, "hello1.txt", F_REG);
    // efs_d_creat(d, "hello3.txt", F_REG);
    // efs_d_lookup(efs_d_namei("/bin"));

    // // // printk("a2\n");
    // efs_d_creat(&root_dentry, "swapfile", F_REG);
    // efs_d_creat(&root_dentry, "swapfile1", F_REG);
    // efs_d_creat(&root_dentry, "swapfile2", F_REG);
    // efs_d_creat(&root_dentry, "swapfile3", F_DIR);
    // efs_d_creat(&root_dentry, "swapfile4", F_REG);
    // efs_d_creat(&root_dentry, "swapfile5", F_REG);
    // efs_d_creat(&root_dentry, "bin", F_DIR);
    // efs_d_lookup(&root_dentry);
    // efs_d_creat(&root_dentry, "bin", F_DIR);
    // efs_d_creat(efs_d_namei("/bin"), "ls", F_REG);
    // efs_d_creat(efs_d_namei("/bin"), "cat", F_REG);
    // efs_d_creat(&root_dentry, "dev", F_DEV);
    
    efs_d_infos(efs_d_namei("/bin"),0);
}

void easy_fs_test()
{
    thread_create(__easy_fs_test, NULL, "blk_test");
}
