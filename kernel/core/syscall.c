#include "syscall.h"
#include "sched.h"
#include "thread.h"
#include "kprintf.h"
#include "io.h"
#include "process.h"

#define IA32_EFER    0xC0000080
#define IA32_STAR    0xC0000081
#define IA32_LSTAR   0xC0000082
#define IA32_FMASK   0xC0000084

#define KERNEL_CS    0x10
#define USER_CS      0x28
#define USER_SS      0x20

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

void syscall_handler(uint64_t sysno, struct registers *regs) {
    switch (sysno) {
    case SYS_exit: {
        kprintf("  [syscall] exit(%d)\n", regs->rdi);
        regs->rax = 0;
        break;
    }
    case SYS_write: {
        kprintf("  [syscall] write(fd=%d, buf=0x%x, len=%d)\n",
                regs->rdi, regs->rsi, regs->rdx);
        regs->rax = regs->rdx;
        break;
    }
    case SYS_yield: {
        schedule();
        regs->rax = 0;
        break;
    }
    case SYS_getpid: {
        struct process *p = process_current();
        regs->rax = p ? p->pid : 0;
        break;
    }
    default:
        kprintf("  [syscall] unknown syscall %d\n", sysno);
        regs->rax = (uint64_t)-1;
        break;
    }
}