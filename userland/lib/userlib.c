#include "userlib.h"

long syscall0(long n) {
    long ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n)
                      : "rcx", "r11", "memory");
    return ret;
}

long syscall1(long n, long a) {
    long ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n), "D"(a)
                      : "rcx", "r11", "memory");
    return ret;
}

long syscall2(long n, long a, long b) {
    long ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n), "D"(a), "S"(b)
                      : "rcx", "r11", "memory");
    return ret;
}

long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n), "D"(a), "S"(b), "d"(c)
                      : "rcx", "r11", "memory");
    return ret;
}

long syscall4(long n, long a, long b, long c, long d) {
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
                      : "rcx", "r11", "memory");
    return ret;
}

/* Wrappers */
void _exit(int status) {
    syscall1(SYS_exit, status);
    for (;;);   /* unreachable */
}

int write(int fd, const void *buf, size_t count) {
    return (int)syscall3(SYS_write, fd, (long)buf, (long)count);
}

int read(int fd, void *buf, size_t count) {
    return (int)syscall3(SYS_read, fd, (long)buf, (long)count);
}

int open(const char *path, int flags) {
    return (int)syscall2(SYS_open, (long)path, flags);
}

int close(int fd) {
    return (int)syscall1(SYS_close, fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)syscall3(SYS_lseek, fd, offset, whence);
}

int fork(void) {
    return (int)syscall0(SYS_fork);
}

void execv(const char *path, const char *const argv[]) {
    syscall2(SYS_exec, (long)path, (long)argv);
    _exit(127);
}

int exec(const char *path, const char *const argv[]) {
    syscall2(SYS_exec, (long)path, (long)argv);
    _exit(127);
    return -1;
}

int wait(int pid) {
    return (int)syscall1(SYS_wait, pid);
}

int getpid(void) {
    return (int)syscall0(SYS_getpid);
}

int getppid(void) {
    return (int)syscall0(SYS_getppid);
}

int readdir(const char *path, int index, char *name, size_t name_len) {
    return (int)syscall4(SYS_readdir, (long)path, index,
                         (long)name, (long)name_len);
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}