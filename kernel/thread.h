#ifndef THREAD_H
#define THREAD_H
#include "types.h"

#define THREAD_READY 0
#define THREAD_RUNNING 1
#define THREAD_BLOCKED 2
#define THREAD_TERMINATED 3

#define THREAD_STACK_SIZE (16 * 1024) // 16 KiB
#define THREAD_NAME_LEN 16

struct thread_context {
    /* General-purpose registers */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    /* Control state */
    uint64_t rip;         /* where the thread resumes */
    uint64_t rflags;
    uint64_t rsp;         /* thread's own stack pointer */
};

struct thread {
    uint64_t id;
    char name[THREAD_NAME_LEN];
    uint32_t state;
    uint32_t _pad;
    void *stack_base;
    uint64_t stack_size;
    struct thread_context ctx;
    uint32_t budget;
    uint32_t _pad2;
    void (*entry)(void);
    struct process *process;
    struct thread *next;
    uint64_t total_ticks;
};

void thread_init_subsystem(void);
struct thread *thread_create(const char *name, void(*entry)(void));
void thread_exit(void) __attribute__((noreturn));
struct thread *thread_current(void);
uint64_t thread_count_total(void);
void thread_dump_all(void);
void set_current_thread(struct thread *t);

/* From kernel/switch.asm */
extern void switch_to(struct thread_context *old_ctx, struct thread_context *new_ctx);



#endif