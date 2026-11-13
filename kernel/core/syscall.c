#include "syscall.h"
#include "sched.h"
#include "kprintf.h"
#include "io.h"
#include "userspace.h"
#include "vmm.h"
#include "terminal.h"
#include "process.h"
#include "thread.h"
#include "fd.h"
#include "kmalloc.h"
#include "vfs.h"
#include "elf.h"

#include <stddef.h>

#define IA32_EFER    0xC0000080
#define IA32_STAR    0xC0000081
#define IA32_LSTAR   0xC0000082
#define IA32_FMASK   0xC0000084

#define KERNEL_CS    0x10
#define USER_CS      0x28
#define USER_SS      0x20

#define PATH_MAX 4096
#define MAX_OPEN_FLAGS 0x10000
#define EXEC_MAX_ARGS 32
#define EXEC_ARG_BYTES 2048

static int copy_from_user(void *kernel_buf, uint64_t user_ptr, uint64_t len);
static int copy_user_string(uint64_t user_ptr, char *kernel_buf,
                            uint64_t max_len);

static int copy_user_argv(uint64_t user_argv, char *storage,
                          uint64_t storage_size, const char **argv) {
    uint64_t used = 0;

    for (uint64_t i = 0; i <= EXEC_MAX_ARGS; i++) {
        uint64_t arg_ptr = 0;
        uint64_t slot = user_argv + i * sizeof(arg_ptr);
        if (slot < user_argv || slot >= USER_SPACE_MAX ||
            sizeof(arg_ptr) > USER_SPACE_MAX - slot ||
            copy_from_user(&arg_ptr, slot, sizeof(arg_ptr)) < 0) {
            return -1;
        }
        if (!arg_ptr) {
            argv[i] = 0;
            return 0;
        }
        if (i == EXEC_MAX_ARGS || used >= storage_size) return -1;

        int len = copy_user_string(arg_ptr, storage + used,
                                   storage_size - used);
        if (len < 0) return -1;
        argv[i] = storage + used;
        used += (uint64_t)len + 1;
    }

    return -1;
}

extern void syscall_entry(void);

_Static_assert(offsetof(struct syscall_regs, rax)    == 112, "syscall_regs.rax");
_Static_assert(offsetof(struct syscall_regs, rip)    == 120, "syscall_regs.rip");
_Static_assert(offsetof(struct syscall_regs, rflags) == 128, "syscall_regs.rflags");
_Static_assert(offsetof(struct syscall_regs, rsp)    == 136, "syscall_regs.rsp");
_Static_assert(sizeof(struct syscall_regs)           == 144, "syscall_regs.size");

void syscall_init(void) {
    uint64_t star = ((uint64_t)(USER_SS - 8) << 48)
                  | ((uint64_t)KERNEL_CS << 32);

    wrmsr(IA32_STAR, star);
    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_FMASK, 0x200 | 0x100 | 0x400 | 0x40000);

    uint64_t efer = rdmsr(IA32_EFER);
    efer |= 1ULL;
    wrmsr(IA32_EFER, efer);

    kprintf("  SYSCALL: initialized\n");
}

void syscall_dump_msrs(void) {
    kprintf("  SYSCALL MSR dump:\n");
    kprintf("    IA32_STAR  = 0x%x\n", rdmsr(IA32_STAR));
    kprintf("    IA32_LSTAR = 0x%x\n", rdmsr(IA32_LSTAR));
    kprintf("    IA32_FMASK = 0x%x\n", rdmsr(IA32_FMASK));
    kprintf("    IA32_EFER  = 0x%x\n", rdmsr(IA32_EFER));
}

static int64_t sys_write(int fd, uint64_t user_buf, uint64_t count) {
    if (user_buf == 0 || count == 0) return -1;

    struct process *p = process_current();
    if (!p) return -1;

    struct file *f = fd_get(&p->fds, fd);
    if (!f) return -1;

    if (!vmm_is_user_range(user_buf, count)) return -1;

    uint64_t to_write = count > 4096 ? 4096 : count;
    uint8_t *bounce = (uint8_t *)kmalloc(to_write);
    if (!bounce) return -1;
    if (copy_from_user(bounce, user_buf, to_write) < 0) {
        kfree(bounce);
        return -1;
    }

    int n = vfs_write(f, bounce, to_write);
    kfree(bounce);
    return n;
}

static int copy_user_string(uint64_t user_ptr, char *kernel_buf,
                            uint64_t max_len) {
    if (!kernel_buf || max_len == 0) return -1;
    if (user_ptr == 0) return -1;
    if (user_ptr >= USER_SPACE_MAX) return -1;

    uint64_t current_page = user_ptr & ~0xFFFULL;
    if (!vmm_is_user_accessible(current_page)) return -1;

    const char *u = (const char *)user_ptr;

    for (uint64_t i = 0; i < max_len - 1; i++) {
        uint64_t addr = user_ptr + i;
        uint64_t page = addr & ~0xFFFULL;

        if (page != current_page) {
            if (!vmm_is_user_accessible(page)) return -1;
            current_page = page;
        }

        char c = u[i];
        kernel_buf[i] = c;
        if (c == 0) return (int)i;
    }
    return -1;
}

static int copy_to_user(uint64_t user_ptr, const void *kernel_buf,
                        uint64_t len) {
    if (!kernel_buf || len == 0) return -1;
    if (user_ptr == 0) return -1;
    if (user_ptr >= USER_SPACE_MAX) return -1;

    uint64_t current_page = user_ptr & ~0xFFFULL;
    if (!vmm_is_user_accessible(current_page)) return -1;

    uint8_t *u = (uint8_t *)user_ptr;
    const uint8_t *k = (const uint8_t *)kernel_buf;

    for (uint64_t i = 0; i < len; i++) {
        uint64_t addr = user_ptr + i;
        uint64_t page = addr & ~0xFFFULL;

        if (page != current_page) {
            if (!vmm_is_user_accessible(page)) return -1;
            current_page = page;
        }

        u[i] = k[i];
    }
    return (int)len;
}

static int copy_from_user(void *kernel_buf, uint64_t user_ptr,
                           uint64_t len) {
    if (!kernel_buf || len == 0) return -1;
    if (user_ptr == 0) return -1;
    if (user_ptr >= USER_SPACE_MAX) return -1;

    uint64_t current_page = user_ptr & ~0xFFFULL;
    if (!vmm_is_user_accessible(current_page)) return -1;

    const uint8_t *u = (const uint8_t *)user_ptr;
    uint8_t *k = (uint8_t *)kernel_buf;

    for (uint64_t i = 0; i < len; i++) {
        uint64_t addr = user_ptr + i;
        uint64_t page = addr & ~0xFFFULL;

        if (page != current_page) {
            if (!vmm_is_user_accessible(page)) return -1;
            current_page = page;
        }

        k[i] = u[i];
    }
    return (int)len;
}

static int64_t sys_open(uint64_t path_user, uint32_t flags) {
    if (path_user == 0) return -1;
    if (flags & ~(uint32_t)MAX_OPEN_FLAGS) return -1;

    char path[PATH_MAX];
    int len = copy_user_string(path_user, path, PATH_MAX);
    if (len < 0) {
        kprintf("  [sys_open] invalid user path\n");
        return -1;
    }

    struct process *p = process_current();
    if (!p) return -1;

    struct file *f = vfs_open(path, flags);
    if (!f) {
        kprintf("  [sys_open] '%s' not found\n", path);
        return -1;
    }

    int fd = fd_alloc(&p->fds, f);
    if (fd < 0) {
        vfs_close(f);
        return -1;
    }

    return fd;
}

static int64_t sys_close(int fd) {
    struct process *p = process_current();
    if (!p) return -1;
    return fd_close(&p->fds, fd);
}

static int64_t sys_read(int fd, uint64_t user_buf, uint64_t count) {
    if (user_buf == 0 || count == 0) return -1;
    if (!vmm_is_user_range(user_buf, count)) return -1;

    struct process *p = process_current();
    if (!p) return -1;

    struct file *f = fd_get(&p->fds, fd);
    if (!f) return -1;

    uint64_t to_read = count > 4096 ? 4096 : count;
    uint8_t *bounce = (uint8_t *)kmalloc(to_read);
    if (!bounce) return -1;

    int n = vfs_read(f, bounce, to_read);
    if (n > 0) {
        if (copy_to_user(user_buf, bounce, (uint64_t)n) < 0) {
            kfree(bounce);
            return -1;
        }
    }
    kfree(bounce);
    return n;
}

static int64_t sys_lseek(int fd, int64_t offset, int whence) {
    struct process *p = process_current();
    if (!p) return -1;

    struct file *f = fd_get(&p->fds, fd);
    if (!f) return -1;

    return (int64_t)vfs_seek(f, (uint64_t)offset, whence);
}

static int64_t sys_readdir(uint64_t path_user, int64_t index,
                            uint64_t name_user, uint64_t name_len) {
    if (!path_user || !name_user || name_len == 0) return -1;
    if (name_len > 256) name_len = 256;

    char path[PATH_MAX];
    if (copy_user_string(path_user, path, sizeof(path)) < 0) return -1;

    struct inode *dir = vfs_lookup(path);
    if (!dir) return -1;
    if (!dir->i_op || !dir->i_op->readdir) return -1;

    char name[256];
    int rc = dir->i_op->readdir(dir, (uint64_t)index, name, sizeof(name));
    if (rc < 0) return -1;

    for (uint64_t i = 0; i < name_len; i++) {
        if (copy_to_user(name_user + i, &name[i], 1) < 0) return -1;
        if (name[i] == 0) break;
    }
    return 0;
}

void syscall_handler(uint64_t sysno, struct syscall_regs *regs) {
    switch (sysno) {
    case SYS_exit: {
        struct thread *t = thread_current();
        struct process *p = process_current();
        if (p) p->exit_status = (uint32_t)regs->rdi;
        if (t && t->user_continuation.rip != 0) {
            userspace_exit_to_kernel();
        } else {
            process_exit((uint32_t)regs->rdi);
        }
        break;
    }
    case SYS_process_exit:
        process_exit((uint32_t)regs->rdi);
        break;
    case SYS_getpid: {
        struct process *p = process_current();
        regs->rax = p ? p->pid : 0;
        break;
    }
    case SYS_getppid: {
        struct process *p = process_current();
        regs->rax = (p && p->parent) ? p->parent->pid : 0;
        break;
    }
    case SYS_wait: {
        pid_t pid = (pid_t)regs->rdi;
        uint32_t status = 0;
        int ret = process_wait(pid, &status);
        if (ret == 0) {
            regs->rax = status;
        } else {
            regs->rax = (uint64_t)-1;
        }
        break;
    }
    case SYS_write: {
        int64_t ret = sys_write((int)regs->rdi, regs->rsi, regs->rdx);
        regs->rax = (uint64_t)ret;
        break;
    }
    case SYS_yield: {
        schedule();
        regs->rax = 0;
        break;
    }
    case SYS_open: {
        int64_t ret = sys_open(regs->rdi, (uint32_t)regs->rsi);
        regs->rax = (uint64_t)ret;
        break;
    }
    case SYS_close: {
        int64_t ret = sys_close((int)regs->rdi);
        regs->rax = (uint64_t)ret;
        break;
    }
    case SYS_read: {
        int64_t ret = sys_read((int)regs->rdi, regs->rsi, regs->rdx);
        regs->rax = (uint64_t)ret;
        break;
    }
    case SYS_lseek: {
        int64_t ret = sys_lseek((int)regs->rdi, (int64_t)regs->rsi, (int)regs->rdx);
        regs->rax = (uint64_t)ret;
        break;
    }
    case SYS_fork: {
        struct process *child = process_fork(regs);
        regs->rax = child ? (uint64_t)child->pid : (uint64_t)-1;
        break;
    }
    
    case SYS_exec: {
    char path[256];
    if (copy_user_string(regs->rdi, path, sizeof(path)) < 0) {
        regs->rax = (uint64_t)-1;
        break;
    }
    const char *kargv[32];
    char kstrings[32][128];
    int kargc = 0;
    uint64_t uargv = regs->rsi;
    if (uargv) {
        for (int i = 0; i < 31; i++) {
            uint64_t uptr = 0;
            if (!vmm_is_user_range(uargv + i * 8, 8)) break;
            const uint64_t *p = (const uint64_t *)(uargv + i * 8);
            uptr = *p;
            if (uptr == 0) break;
            if (copy_user_string(uptr, kstrings[i], sizeof(kstrings[i])) < 0) break;
            kargv[i] = kstrings[i];
            kargc++;
        }
    }
    kargv[kargc] = 0;

    process_exec(path, kargv);
    break;
    }

    case SYS_readdir: {
    int64_t ret = sys_readdir(regs->rdi, (int64_t)regs->rsi, regs->rdx, regs->r10);
    regs->rax = (uint64_t)ret;
    break;
    }

    default:
        kprintf("  [syscall] unknown syscall %d\n", sysno);
        regs->rax = (uint64_t)-1;
        break;
    }
}