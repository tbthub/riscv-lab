#include "std/stddef.h"
#include "core/proc.h"
#include "lib/string.h"

extern int do_debug(int a0, const char *a1, const void *a2, char *const a3[], uint64 a4, int a5);
extern int do_fork();
extern int do_exec(const char *path, char *const argv[]);
extern int do_exit(int status) __attribute__((noreturn)); // 退出状态码
extern int do_sleep(uint64 ticks);
extern int do_wait(int *status);
extern int do_pipe();
extern int do_kill(int pid, int status);
extern int do_fstat();
extern int do_chdir(const char *path);
extern int do_dup();
extern int do_getpid();
extern int do_sbrk();
extern int do_uptime();
extern int do_open(const char *path, int flags, int mode);
extern int do_close(int fd);
extern int do_read(int fd, const void *buf, uint64 count);
extern int do_write(int fd, const void *buf, uint64 count);
extern int do_mknod(const char *path, int mode, int dev);
extern int do_unlink(const char *path);
extern int do_link();
extern int do_mkdir(const char *path);

// * 请确保在 trapframe 结构体中顺序放置 a0->a6
static void get_args(uint64 *args, int n)
{
    struct thread_info *p = myproc();
    memcpy(args, &p->tf->a0, n * sizeof(uint64));
}

int sys_debug()
{
    uint64 args[6];
    get_args(args, 6);
    return do_debug((int)args[0], (const char *)args[1], (const void *)args[2],
                    (char *const *)args[3], (uint64)args[4], (int)args[5]);
}

int sys_fork()
{
    return do_fork();
}

int sys_exec()
{
    uint64 args[2];
    get_args(args, 2);
    return do_exec((const char *)args[0], (char *const *)args[1]);
}

__attribute__((noreturn)) int sys_exit()
{
    uint64 args[1];
    get_args(args, 1);
    do_exit((int)args[0]);
}

int sys_wait()
{
    uint64 args[1];
    get_args(args, 1);
    return do_wait((int *)args[0]);
}

int sys_pipe()
{
    return do_pipe();
}

int sys_read()
{
    uint64 args[3];
    get_args(args, 3);
    return do_read((int)args[0], (void *)args[1], args[2]);
}

int sys_kill()
{
    uint64 args[2];
    get_args(args, 2);
    return do_kill((int)args[0], (int)args[1]);
}

int sys_fstat()
{
    return do_fstat();
}

int sys_chdir()
{
    uint64 args[1];
    get_args(args, 1);
    return do_chdir((const char *)args[0]);
}

int sys_dup()
{
    return do_dup();
}

int sys_getpid()
{
    return do_getpid();
}

int sys_sbrk()
{
    return do_sbrk();
}

int sys_sleep()
{
    uint64 args[1];
    get_args(args, 1);
    return do_sleep(args[0]);
}

int sys_uptime()
{
    return do_uptime();
}

int sys_open()
{
    uint64 args[3];
    get_args(args, 3);
    return do_open((const char *)args[0], (int)args[1], (int)args[2]);
}

int sys_write()
{
    uint64 args[3];
    get_args(args, 3);
    return do_write((int)args[0], (const void *)args[1], args[2]);
}

int sys_mknod()
{
    uint64 args[3];
    get_args(args, 3);
    return do_mknod((const char *)args[0], (int)args[1], (int)args[2]);
}

int sys_unlink()
{
    uint64 args[1];
    get_args(args, 1);
    return do_unlink((const char *)args[0]);
}

int sys_link()
{
    return do_link();
}

int sys_mkdir()
{
    uint64 args[1];
    get_args(args, 1);
    return do_mkdir((const char *)args[0]);
}

int sys_close()
{
    uint64 args[1];
    get_args(args, 1);
    return do_close((int)args[0]);
}