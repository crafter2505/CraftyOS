#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "syscall_regs.h"

void syscall_init(void);
void syscall_handler(uint64_t sysno, struct syscall_regs *regs);

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

void syscall_dump_msrs(void);


#endif