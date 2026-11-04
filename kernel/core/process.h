#ifndef PROCESS_H
#define PROCESS_H
#include "types.h"
#include "fd.h"
#include "isr.h"

typedef uint64_t pid_t;
#define PROCESS_NAME_LEN 16

struct process {
    pid_t pid;
    char name[PROCESS_NAME_LEN];
    uint32_t state;
    uint32_t _pad;
    uint64_t pml4_phys;
    uint64_t last_code_phys;
    uint32_t exit_status;
    uint32_t exit_called;
    struct thread *waiting_parent_thread;
    struct thread *threads;
    struct fd_table fds;
    struct process *parent;
    struct process *next;
    uint64_t total_ticks;
};

#define FF_RAX     0
#define FF_RBX     8
#define FF_RCX     16
#define FF_RDX     24
#define FF_RSI     32
#define FF_RDI     40
#define FF_RBP     48
#define FF_R8      56
#define FF_R9      64
#define FF_R10     72
#define FF_R11     80
#define FF_R12     88
#define FF_R13     96
#define FF_R14     104
#define FF_R15     112
#define FF_RIP     120
#define FF_RFLAGS  128
#define FF_RSP     136

#include "syscall_regs.h"

struct fork_frame {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
};


void process_init_subsystem(void);
struct process *process_create(const char *name, void (*entry)(void *), void *arg);
struct process *process_current(void);
void process_exit(uint32_t status) __attribute__((noreturn));
int process_wait(pid_t pid, uint32_t *status_out);
void process_free_address_space(uint64_t pml4_phys);
uint64_t process_count(void);
struct process *process_fork(struct syscall_regs *regs);
void process_dump_all(void);


#endif