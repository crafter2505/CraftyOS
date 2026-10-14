#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "isr.h"

void syscall_init(void);
void syscall_handler(uint64_t sysno, struct registers *regs);

#define SYS_exit    0
#define SYS_write   1
#define SYS_yield   2
#define SYS_getpid  3

void syscall_dump_msrs(void);


#endif