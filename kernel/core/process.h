#ifndef PROCESS_H
#define PROCESS_H
#include "types.h"

typedef uint64_t pid_t;
#define PROCESS_NAME_LEN 16

struct process {
    pid_t pid;
    char name[PROCESS_NAME_LEN];
    uint32_t state;
    uint32_t _pad;
    uint64_t pml4_phys;
    struct thread *threads;
    struct process *parent;
    struct process *next;
    uint64_t total_ticks;
};

void process_init_subsystem(void);
struct process *process_create(const char *name, void (*entry)(void *), void *arg);
struct process *process_current(void);
void process_dump_all(void);


#endif