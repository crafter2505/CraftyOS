#ifndef USERLIB_H
#define USERLIB_H

typedef unsigned long  size_t;
typedef long           ssize_t;
typedef long           off_t;

#define SYS_exit          0
#define SYS_write         1
#define SYS_yield         2
#define SYS_getpid        3
#define SYS_getppid       4
#define SYS_wait          5
#define SYS_process_exit  6
#define SYS_open          7
#define SYS_close         8
#define SYS_read          9
#define SYS_lseek         10
#define SYS_fork          11
#define SYS_exec          12
#define SYS_readdir       13

long syscall0(long n);
long syscall1(long n, long a);
long syscall2(long n, long a, long b);
long syscall3(long n, long a, long b, long c);
long syscall4(long n, long a, long b, long c, long d);

int main(int argc, char **argv);
void _exit(int status) __attribute__((noreturn));
int  write(int fd, const void *buf, size_t count);
int  read(int fd, void *buf, size_t count);
int  open(const char *path, int flags);
int  close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int  fork(void);
void execv(const char *path, const char *const argv[]) __attribute__((noreturn));
int  exec(const char *path, const char *const argv[])
    __attribute__((noreturn));int  wait(int pid);
int  getpid(void);
int  getppid(void);
int  readdir(const char *path, int index, char *name, size_t name_len);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);

#endif