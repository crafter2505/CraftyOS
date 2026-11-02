#ifndef PROCESS_H
#define PROCESS_H
#include "types.h"
#include "fd.h"

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

void process_init_subsystem(void);
struct process *process_create(const char *name, void (*entry)(void *), void *arg);
struct process *process_current(void);
void process_exit(uint32_t status) __attribute__((noreturn));
int process_wait(pid_t pid, uint32_t *status_out);
void process_free_address_space(uint64_t pml4_phys);
void process_dump_all(void);
uint64_t process_count(void);


#endif