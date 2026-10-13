#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

void syscall_init(void);

#define SYS_exit    0
#define SYS_write   1
#define SYS_yield   2
#define SYS_getpid  3

void syscall_dump_msrs(void);


#endif