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

    // efs_d_infos(efs_d_namei("/bin"),0);
    // efs_d_info(&root_dentry);
    // efs_d_info();

    // efs_d_creat(efs_d_namei("/"), "bin", F_DIR);
    // struct easy_dentry *bin = efs_d_namei("/bin");
    // efs_d_creat(bin, "ls", F_DIR);
    // struct easy_dentry *rd = efs_d_namei("/");
    // efs_d_infos(rd);
    // printk("--------\n");
    // efs_d_lookup(rd);
    // char str[] = "This is bash context";
    // // efs_d_write_name("/swapfile", 0, sizeof(str), str);

    // char *vaddr = __alloc_page(0);
    // efs_d_read_name("/bin/bash", 0, sizeof(str), vaddr);
    // printk("%s\n", vaddr);

    // efs_d_creat(efs_d_namei("/bin"), "bash", F_REG);
    // efs_d_write_name("/bin/bash", 0, sizeof(str), str);

    // struct easy_dentry *rd = efs_d_namei("/");
    // printk("Root directory info before:\n");
    // efs_d_infos(rd);
    // printk("--------\n");

    // // 创建 /bin 目录
    // printk("Creating /bin directory...\n");
    // struct easy_dentry *bin_d = efs_d_creat(rd, "bin", F_DIR);
    // if (!bin_d)
    // {
    //     printk("Failed to create /bin!\n");
    //     return;
    // }

    // // 在 /bin 下创建 bash、ls、echo
    // efs_d_creat(bin_d, "bash", F_REG);
    // efs_d_creat(bin_d, "ls", F_REG);
    // efs_d_creat(bin_d, "echo", F_REG);

    // // 写入 /bin/bash
    // char str1[] = "This is bash content";
    // efs_d_write_name("/bin/bash", 0, sizeof(str1), str1);

    // // 写入 /bin/ls 和 /bin/echo
    // char str2[] = "LS command";
    // char str3[] = "Echo command";
    // efs_d_write_name("/bin/ls", 0, sizeof(str2), str2);
    // efs_d_write_name("/bin/echo", 0, sizeof(str3), str3);

    // // 创建 /bin/utils 目录
    // printk("Creating /bin/utils directory...\n");
    // struct easy_dentry *utils_d = efs_d_creat(bin_d, "utils", F_DIR);
    // if (!utils_d)
    // {
    //     printk("Failed to create /bin/utils!\n");
    //     return;
    // }

    // // 在 /bin/utils 目录下创建 hello 文件
    // efs_d_creat(utils_d, "hello", F_REG);
    // char hello_str[] = "Hello World!";
    // efs_d_write_name("/bin/utils/hello", 0, sizeof(hello_str), hello_str);

    // // 读取 /bin/bash
    // char *vaddr = __alloc_page(0);
    // efs_d_read_name("/bin/bash", 0, sizeof(str1), vaddr);
    // printk("Read from /bin/bash: %s\n", vaddr);

    // // 读取 /bin/ls
    // efs_d_read_name("/bin/ls", 0, sizeof(str2), vaddr);
    // printk("Read from /bin/ls: %s\n", vaddr);

    // // 读取 /bin/echo
    // efs_d_read_name("/bin/echo", 0, sizeof(str3), vaddr);
    // printk("Read from /bin/echo: %s\n", vaddr);

    // // 读取 /bin/utils/hello
    // efs_d_read_name("/bin/utils/hello", 0, sizeof(hello_str), vaddr);
    // printk("Read from /bin/utils/hello: %s\n", vaddr);

    // // 测试 /swapfile
    // printk("Creating /swapfile...\n");
    // efs_d_creat(rd, "swapfile", F_REG);
    // char swap_str[] = "This is swapfile data";
    // efs_d_write_name("/swapfile", 0, sizeof(swap_str), swap_str);
    // efs_d_read_name("/swapfile", 0, sizeof(swap_str), vaddr);
    // printk("Read from /swapfile: %s\n", vaddr);

    // // 遍历根目录，检查所有文件
    // printk("Root directory info after:\n");
    // efs_d_infos(rd);

    // efs_d_creat(rd,"mnt",F_DIR);
    // rd = rd->d_parent;
    // efs_d_creat(rd, "bin", F_DIR);
    // efs_d_creat(rd, "dev", F_DEV);
    // efs_d_creat(rd, "swapfile", F_REG);
    // efs_d_creat(efs_d_namei("/dev"),"tty",F_DEV);
    // efs_d_info(rd);
    // efs_d_infos(rd);

    // efs_d_lookup(efs_d_namei("/"));
    // printk("---------\n");
    // efs_d_creat(efs_d_namei("/"), "dev", F_REG);
    // efs_d_lookup(efs_d_namei("/"));
    // printk("---------\n");
    // efs_d_creat(efs_d_namei("/bin"), "hello", F_DIR);
    // efs_d_infos(efs_d_namei("/"), 3);
}

void easy_fs_test()
{
    thread_create(__easy_fs_test, NULL, "easy_fs_test");
}
