#ifndef USERSPACE_H
#define USERSPACE_H

#include "types.h"

#define USER_CODE_BASE   0x0000008000000000ULL
#define USER_STACK_TOP   0x0000008000008000ULL
#define USER_STACK_SIZE  0x2000ULL

struct kernel_continuation {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
};

int enter_userspace(uint64_t entry, uint64_t user_stack_top);

void userspace_exit_to_kernel(void) __attribute__((noreturn));
int userspace_run_embedded(void);

#endif