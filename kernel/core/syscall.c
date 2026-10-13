#include "syscall.h"
#include "io.h"
#include "kprintf.h"

#define IA32_EFER  0xC0000080
#define IA32_STAR  0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084

#define KERNEL_CS 0x10
#define USER_SS   0x20

extern void syscall_entry(void);

void syscall_init(void) {
    uint64_t star = ((uint64_t)(USER_SS - 8) << 48)
                  | ((uint64_t)KERNEL_CS << 32);
    uint64_t fmask = 0x200 | 0x100 | 0x400 | 0x40000;

    wrmsr(IA32_STAR, star);
    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_FMASK, fmask);
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | 1ULL);

    kprintf("  SYSCALL: initialized\n");
    kprintf("    IA32_STAR  = 0x%x\n", star);
    kprintf("    IA32_LSTAR = 0x%x\n", (uint64_t)syscall_entry);
    kprintf("    IA32_FMASK = 0x%x\n", fmask);
}

void syscall_dump_msrs(void) {
    kprintf("  SYSCALL MSR dump:\n");
    kprintf("    IA32_STAR  = 0x%x\n", rdmsr(IA32_STAR));
    kprintf("    IA32_LSTAR = 0x%x\n", rdmsr(IA32_LSTAR));
    kprintf("    IA32_FMASK = 0x%x\n", rdmsr(IA32_FMASK));
    kprintf("    IA32_EFER  = 0x%x\n", rdmsr(IA32_EFER));
}