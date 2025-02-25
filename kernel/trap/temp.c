#include "std/stddef.h"
#include "core/proc.h"

int do_debug(int a0, const char *a1, const void *a2, char *const a3[], uint64 a4, int a5)
{
    // printk("sys_debug->do_debug args:\n");
    // printk("a0: %d\n", a0);
    // printk("a1: %s\n", a1);
    // printk("a2: %p\n", a2);

    // for (int i = 0;; i++)
    // {
    //     if (a3[i] == NULL)
    //         break;
    //     printk("a3: %s\n", a3[i]);
    // }

    // printk("a4: %p\n", a4);
    // printk("a5: %d\n", a5);
    return -ENOSYS;
}

int do_fork()
{
    return -ENOSYS;
}

int do_exec(const char *path, char *const argv[])
{
    return -ENOSYS;
}

int do_exit(int status)
{
    return -ENOSYS;
}

int do_sleep(uint64 ticks)
{
    return -ENOSYS;
}

int do_wait(int *status)
{
    return -ENOSYS;
}

int do_pipe()
{
    return -ENOSYS;
}

int do_kill(int pid, int status)
{
    return -ENOSYS;
}

int do_fstat()
{
    return -ENOSYS;
}

int do_chdir(const char *path)
{
    return -ENOSYS;
}

int do_dup()
{
    return -ENOSYS;
}

int do_getpid()
{
    return -ENOSYS;
}

int do_sbrk()
{
    return -ENOSYS;
}

int do_uptime()
{
    return -ENOSYS;
}

int do_open(const char *path, int flags, int mode)
{
    return -ENOSYS;
}

int do_close(int fd)
{
    return -ENOSYS;
}

int do_read(int fd, void *buf, uint64 count)
{
    return -ENOSYS;
}

int do_write(int fd, const void *buf, uint64 count)
{
    return -ENOSYS;
}

int do_mknod(const char *path, int mode, int dev)
{
    return -ENOSYS;
}

int do_unlink(const char *path)
{
    return -ENOSYS;
}

int do_link()
{
    return -ENOSYS;
}

int do_mkdir()
{
    return -ENOSYS;
}

int do_mmap()
{
    return -ENOSYS;
}

int do_munmap()
{
    return -ENOSYS;
}