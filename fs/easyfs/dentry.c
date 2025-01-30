#include "easyfs.h"
#include "utils/string.h"
#include "dev/blk/blk_dev.h"
static const char t[] = "\0-db"; // t[1] = '-', t[2] = 'd', t[3] = 'b'
struct easy_dentry root_dentry = {
    .d_dd.d_ino = 1,
    .d_dd.d_name = "/",
    .d_dd.d_type = F_DIR,
};

// *
// ! 锁 锁  不支持挂载
// ?
// TODO

/*
 * 这里简单实现目录功能，但是效率比较低
 *
 * 目录文件在磁盘的存放内容我们简单实现为 easy_dirent[]
 * 1. D：将其从 dentry 链移除
 * 2. C：插入到 dentry 链
 * 3. R：遍历 dentry 链
 * 4. U: 直接该即可
 * 目录文件修改后会写回磁盘，后面就由 flush 控制了
 *
 */

// 把 dentry 加到超级块链表中,需要加锁
static inline void efs_d_addsb(struct easy_dentry *d)
{
    hash_add_head(&m_esb.s_dhash, strhash(d->d_dd.d_name), &d->d_hnode);
    list_add_head(&d->d_list, &m_esb.s_dlist);
}

static inline int efs_d_namecmp(const char *s, const char *t)
{
    return strncmp(s, t, DIR_MAXLEN);
}

static struct easy_dentry *efs_d_alloc()
{
    struct easy_dentry *d = kmem_cache_alloc(&efs_dentry_kmem_cache);
    assert(d != NULL, "efs_d_alloc");

    d->d_inode = NULL;
    d->d_parent = NULL;
    INIT_LIST_HEAD(&d->d_child);
    INIT_LIST_HEAD(&d->d_sibling);

    INIT_HASH_NODE(&d->d_hnode);
    INIT_LIST_HEAD(&d->d_list);
    INIT_LIST_HEAD(&d->d_dirty);

    spin_init(&d->d_lock, "dentry");
    sleep_init(&d->d_slock, "dentry");

    d->d_flags = 0;
    atomic_set(&d->refcnt, 0);
    return d;
}

// 移除相关内存结构，不过并没有释放 inode
static void efs_d_free(struct easy_dentry *d)
{
    if (!d)
        return;

    spin_lock(&m_esb.s_lock);

    list_del(&d->d_list);
    hash_del_node(&m_esb.s_dhash, &d->d_hnode);

    spin_unlock(&m_esb.s_lock);

    list_del(&d->d_child);
    list_del(&d->d_sibling);

    kmem_cache_free(&efs_dentry_kmem_cache, d);
}

static inline void efs_d_conn(struct easy_dentry *d, struct easy_dentry *pd)
{
    d->d_parent = pd;
    list_add_tail(&d->d_sibling, &pd->d_child);
}

// * 从磁盘中将目录的信息读出，填充内存中的目录项
// @param pd: 父目录项
static void efs_d_fill(struct easy_dentry *pd)
{
    // 先获取到对应的 inode
    struct easy_m_inode *pi = efs_i_get(pd->d_dd.d_ino);
    pd->d_inode = pi;
    int d_cnt = pi->i_di.i_size / sizeof(struct easy_dirent);
    for (int i = 0; i < d_cnt; i++)
    {
        struct easy_dentry *d = efs_d_alloc();
        efs_i_read(pi, i * sizeof(struct easy_dirent), sizeof(struct easy_dirent), &d->d_dd);
        efs_d_conn(d, pd);

        spin_lock(&m_esb.s_lock);
        efs_d_addsb(d);
        spin_unlock(&m_esb.s_lock);
    }
}

// 查重
// * 需要加锁
// 返回值 重复对象的目录项指针。 NULL：通过  else：未通过（含有同名）
static struct easy_dentry *efs_d_name_check(struct easy_dentry *pd, const char *name)
{
    struct easy_dentry *d = NULL;
    if (!TEST_FLAG(&pd->d_flags, D_CHILD)) // 如果没有填充子目录
    {
        efs_d_fill(pd);
        SET_FLAG(&pd->d_flags, D_CHILD);
    }

    list_for_each_entry(d, &pd->d_child, d_sibling)
    {
        if (efs_d_namecmp(d->d_dd.d_name, name) == 0)
            return d;
    }
    return NULL;
}

// 返回子目录里面 name 的目录项
static inline struct easy_dentry *efs_d_namex(struct easy_dentry *pd, const char *name)
{
    return efs_d_name_check(pd, name);
}

// TODO 结合 inode 描述的性能问题
static void efs_d_write(struct easy_dentry *pd)
{
    struct easy_dentry *d;
    int offset = 0;

    if (!pd->d_inode)
        pd->d_inode = efs_i_get(pd->d_dd.d_ino);

    list_for_each_entry(d, &pd->d_child, d_sibling)
    {
        efs_i_write(pd->d_inode, offset, sizeof(struct easy_dirent), &d->d_dd);
        offset += sizeof(struct easy_dirent);
    }
}

static const char efs_d_type_tostr(enum easy_file_type type)
{
    return t[type]; // 直接返回 t 数组的地址
}

inline void efs_d_info(const struct easy_dentry *d)
{
    if (!d)
        return;
    printk("%c\t%s\n", efs_d_type_tostr(d->d_dd.d_type), d->d_dd.d_name);
}

// * (这个函数可能会陷入睡眠)
void efs_d_lookup(struct easy_dentry *pd)
{
    struct easy_dentry *d = NULL;
    sleep_on(&pd->d_slock);
    if (!TEST_FLAG(&pd->d_flags, D_CHILD)) // 如果没有填充子目录
    {
        efs_d_fill(pd);
        SET_FLAG(&pd->d_flags, D_CHILD);
    }

    list_for_each_entry(d, &pd->d_child, d_sibling)
        efs_d_info(d);

    wake_up(&pd->d_slock);
}

void efs_d_creat(struct easy_dentry *pd, const char *name, enum easy_file_type type)
{
    sleep_on(&pd->d_slock);
    if (efs_d_name_check(pd, name))
    {
        printk("Create a file with the same name: %s\n", name);
        wake_up(&pd->d_slock);
        return;
    }

    struct easy_m_inode *new_i = efs_i_new();
    struct easy_dentry *new_d = efs_d_alloc();

    new_d->d_dd.d_ino = new_i->i_di.i_no;
    new_d->d_dd.d_type = type;
    strncpy(new_d->d_dd.d_name, name, DIR_MAXLEN);
    efs_i_type(new_i, type);

    efs_d_conn(new_d, pd);
    efs_d_write(pd);
    wake_up(&pd->d_slock);
}

// int efs_d_free(struct easy_dentry *d)
// {
//     if (!d)
//         return;
// }

int efs_d_unlink(struct easy_dentry *pd, struct easy_dentry *d)
{
    if (!pd || !d)
        return -1; // 目标不存在

    if (d->d_dd.d_type == F_DIR && !list_empty(&d->d_child))
        return -1; // 不能删除非空目录

    // 从父目录的子目录链表中移除
    list_del(&d->d_sibling);

    // 释放 inode 和 dentry 资源
    efs_inode_free(d->d_inode);

    return 0; // 删除成功
}

struct easy_dentry *efs_d_namei(const char *path)
{
    struct easy_dentry *d;
    char *p, *tmp, *name;

    spin_lock(&m_esb.s_lock);
    hash_for_each_entry(d, &m_esb.s_dhash, strhash(path), d_hnode)
    {
        if (!efs_d_namecmp(d->d_dd.d_name, path))
        {
            spin_unlock(&m_esb.s_lock);
            return d;
        }
    }
    spin_unlock(&m_esb.s_lock);

    tmp = p = kmalloc(MAX_PATH_LEN + 1, 0);

    strdup(p, path);
    // 如果是绝对路径
    // TODO 暂时只支持绝对路径，以后改
    if (p[0] == '/')
    {
        d = &root_dentry;
        p++;
    }

    else // 如果是相对路径
    {
        // TODO 相对路径：从进程的 pwd 开始
    }
    name = p;

    while (*p)
    {
        if (*p == '/')
        {
            *p = '\0'; // 截断
            d = efs_d_namex(d, name);
            if (!d) // 找不到路径，返回 NULL
                return NULL;

            name = p + 1; // 移动到下一个目录名
        }
        p++;
    }

    // 解析最后一级目录
    if (*name)
        d = efs_d_namex(d, name);
    kfree(tmp);
    return d;
}

// * 必须在 efs_i_root_init 之后
void efs_d_root_init()
{
    assert(root_m_inode.i_sb != NULL, "efs_d_root_init\n");

    root_dentry.d_inode = NULL;
    root_dentry.d_parent = NULL;
    INIT_LIST_HEAD(&root_dentry.d_child);
    INIT_LIST_HEAD(&root_dentry.d_sibling);

    INIT_HASH_NODE(&root_dentry.d_hnode);
    INIT_LIST_HEAD(&root_dentry.d_list);
    INIT_LIST_HEAD(&root_dentry.d_dirty);

    spin_init(&root_dentry.d_lock, "dentry");
    sleep_init(&root_dentry.d_slock, "dentry");

    root_dentry.d_flags = 0;
    efs_d_fill(&root_dentry);
    SET_FLAG(&root_dentry.d_flags, D_CHILD);
}

static inline void __efs_d_infos(struct easy_dentry *d, int indent, int level)
{

    // 打印当前目录项
    for (int i = 0; i < indent; i++)
        printk(" ");
    printk("%s\n", d->d_dd.d_name); // 目录后面加 "/" 以区分
    if (level == 0)
        return;
    // 确保目录已填充子项
    if (d->d_dd.d_type == F_DIR)
    {
        if (!TEST_FLAG(&d->d_flags, D_CHILD))
        {
            efs_d_fill(d);
            SET_FLAG(&d->d_flags, D_CHILD);
        }
    }

    // 遍历子目录项
    struct easy_dentry *d1;
    list_for_each_entry(d1, &d->d_child, d_sibling)
    {
        if (d1->d_dd.d_type == F_DIR)
        {
            // 递归打印子目录
            __efs_d_infos(d1, indent + 3, level - 1);
        }
        else
        {
            // 打印普通文件
            for (int i = 0; i < indent + 3; i++)
                printk(" ");
            printk("%s\n", d1->d_dd.d_name);
        }
    }
}

// * 用于调试
void efs_d_infos(struct easy_dentry *d, int level)
{
    if (level == -1)
        level = 0x7FFFFFFF;
    __efs_d_infos(d, 0, level);
}
