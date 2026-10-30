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

#define IA32_EFER    0xC0000080
#define IA32_STAR    0xC0000081
#define IA32_LSTAR   0xC0000082
#define IA32_FMASK   0xC0000084

#define KERNEL_CS    0x10
#define USER_CS      0x28
#define USER_SS      0x20

#define PATH_MAX 4096
#define MAX_OPEN_FLAGS 0x10000

extern void syscall_entry(void);


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

static int64_t sys_write(int fd, uint64_t buf, uint64_t len) {
    if (fd != 1) {
        kprintf("  [sys_write] unsupported fd=%d\n", (int64_t)fd);
    }
    if (!vmm_is_user_range(buf, len)) {
        kprintf("  [sys)write] invalid user range: buf0x%x len=%d\n",
        buf, len);
        return -1;
    }
    const char *user_buf = (const char *)buf;
    for (uint64_t i = 0; i < len; i++) {
        terminal_putchar(user_buf[i]);
    }
    return (int64_t)len;
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

void syscall_handler(uint64_t sysno, struct registers *regs) {
    switch (sysno) {
        case SYS_exit: {
        struct process *p = process_current();
        if (p) p->exit_status = (uint32_t)regs->rdi;
        userspace_exit_to_kernel();
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
        int64_t ret = sys_write((int)regs->rdi,
                                regs->rsi,
                                regs->rdx);
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
    default:
        kprintf("  [syscall] unknown syscall %d\n", sysno);
        regs->rax = (uint64_t)-1;
        break;
    }
}