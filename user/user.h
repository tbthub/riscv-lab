#ifndef __USER_H__
#define __USER_H__
typedef unsigned long uint64;

// 系统调用
extern int debug(int a0, const char *a1, const void *a2, char *const a3[], uint64 a4, int a5);
extern int fork(void);
extern int exit(int,int,int,int,int) __attribute__((noreturn));
extern int wait(int *);
extern int pipe(int *);
extern int write(int, const void *, int);
extern int read(int, void *, int);
extern int close(int);
extern int kill(int);
extern int exec(const char *, char **);
extern int open(const char *, int);
extern int mknod(const char *, short, short);
extern int unlink(const char *);
// extern  int fstat(int fd, struct stat *);
extern int link(const char *, const char *);
extern int mkdir(const char *);
extern int chdir(const char *);
extern int dup(int);
extern int getpid(void);
extern char *sbrk(int);
extern int sleep(int);
extern int uptime(void);

#endif