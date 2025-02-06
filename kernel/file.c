#include "fs/file.h"
#include "../fs/easyfs/easyfs.h"
#include "mm/slab.h"

static struct file *file_alloc()
{
    struct file *f = kmem_cache_alloc(&file_kmem_cache);
    f->f_flags = 0;
    f->f_ip = NULL;
    f->f_off = 0;
    atomic_set(&f->f_ref, 0);
    mutex_init(&f->f_mutex, "file f_mutex");
    return f;
}

static inline void file_free(struct file *f)
{
    assert(f != NULL, "file_free\n");
    kmem_cache_free(&file_kmem_cache, f);
}

struct file *file_dup(struct file *f)
{
    assert(atomic_read(&f->f_ref) > 0, "file_dup\n");
    atomic_inc(&f->f_ref);
    return f;
}

struct file *file_open(const char *file_path, flags_t flags)
{
    struct easy_m_inode *i = efs_d_namei(file_path);
    if (!i)
    {
        printk("file_open efs_d_namei\n");
        return NULL;
    }
    struct file *f = file_alloc();
    atomic_set(&f->f_ref, 1);
    SET_FLAG(&f->f_flags, flags);
    f->f_ip = i;
    f->f_off = 0;
    return f;
}

void file_close(struct file *f)
{
    assert(atomic_read(&f->f_ref) > 0, "file_close");
    // 如果自减后为 0，则可以释放
    if (atomic_dec_and_test(&f->f_ref))
    {
        efs_i_put(f->f_ip);
        file_free(f);
    }
    // 有其他线程也在引用，则仅仅减
}

int file_read(struct file *f, void *vaddr, uint32 len)
{

    int r = 0;
    if (!TEST_FLAG(&f->f_flags, FILE_READ))
    {
        printk("File is unreadable\n");
        return -1;
    }

    assert(f->f_ip != NULL, "file_read f->f_ip\n");
    mutex_lock(&f->f_mutex);
    if ((r = efs_i_read(f->f_ip, f->f_off, len, vaddr)) > 0)
        f->f_off += r;
    else
        panic("file_read\n");
    mutex_unlock(&f->f_mutex);
    return r;
}

int file_write(struct file *f, void *vaddr, uint32 len)
{
    int r = 0;
    if (!TEST_FLAG(&f->f_flags, FILE_WRITE))
    {
        printk("File not writable\n");
        return -1;
    }
    assert(f->f_ip != NULL, "file_write f->f_ip\n");
    mutex_lock(&f->f_mutex);
    if ((r = efs_i_write(f->f_ip, f->f_off, len, vaddr)) > 0)
        f->f_off += r;
    else
        panic("file_write\n");
    mutex_unlock(&f->f_mutex);
    return r;
}

int file_llseek(struct file *f, uint32 offset, int whence)
{
    assert(f->f_ip != NULL, "file_llseek f->f_ip\n");

    mutex_lock(&f->f_mutex); // 锁定文件操作

    int ret = 0;
    uint32 new_offset;
    switch (whence)
    {
    case SEEK_SET: // 从文件开头设置偏移量
        if (offset > efs_i_size(f->f_ip))
        {
            printk("file_llseek: offset > f->f_ip->i_di.i_size\n");
            ret = -1;
        }
        else
            f->f_off = offset;
        break;

    case SEEK_CUR: // 从当前偏移量设置
        new_offset = f->f_off + offset;
        if (new_offset > efs_i_size(f->f_ip) || new_offset < 0)
        {
            printk("file_llseek: new_offset out of bounds\n");
            ret = -1;
        }
        else
            f->f_off = new_offset;
        break;

    case SEEK_END: // 从文件末尾设置偏移量
        new_offset = efs_i_size(f->f_ip) + offset;
        if (new_offset > efs_i_size(f->f_ip) || new_offset < 0)
        {
            printk("file_llseek: new_offset out of bounds\n");
            ret = -1;
        }
        else
            f->f_off = new_offset;
        break;

    default:
        printk("file_llseek: invalid whence value\n");
        ret = -1;
        break;
    }

    mutex_unlock(&f->f_mutex); // 解锁文件操作
    return ret;
}
