#ifndef USERSPACE_H
#define USERSPACE_H

#include "types.h"
#include "thread.h"

#define USER_CODE_BASE   0x0000008000000000ULL
#define USER_STACK_TOP   0x0000008000008000ULL
#define USER_STACK_SIZE  0x2000ULL

int enter_userspace(struct thread *self, uint64_t entry, uint64_t user_stack_top);
void userspace_exit_to_kernel(void) __attribute__((noreturn));
int userspace_run_program(const void *prog_start, uint64_t prog_size);
int userspace_run_embedded(void);

#endif