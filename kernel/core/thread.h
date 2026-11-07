#ifndef THREAD_H
#define THREAD_H
#include "types.h"

#define THREAD_READY        0
#define THREAD_RUNNING      1
#define THREAD_BLOCKED      2
#define THREAD_SLEEPING     3
#define THREAD_TERMINATED   4

#define THREAD_STACK_SIZE (16 * 1024)
#define THREAD_SYSCALL_STACK_SIZE (8 * 1024)
#define THREAD_NAME_LEN 16

struct thread_context {
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
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
};

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

struct thread {
    uint64_t id;
    char name[THREAD_NAME_LEN];
    uint32_t state;
    uint32_t _pad;
    void *stack_base;
    uint64_t stack_size;
    void *syscall_stack;
    uint64_t syscall_stack_size;
    struct thread_context ctx;
    struct kernel_continuation user_continuation;
    uint32_t budget;
    uint32_t _pad2;
    void (*entry)(void *);
    void *arg;
    struct process *process;
    struct thread *next;
    struct thread *next_dead;
    uint64_t total_ticks;
    uint64_t wake_at_tick;
};

void thread_init_subsystem(void);
struct thread *thread_create(const char *name, void (*entry)(void *), void *arg);
void thread_exit(void) __attribute__((noreturn));
struct thread *thread_current(void);
uint64_t thread_count_total(void);
void thread_dump_all(void);
void set_current_thread(struct thread *t);
void thread_reap_pending(void);
void thread_mark_dead_and_unlink(void);

extern void switch_to(struct thread_context *old_ctx, struct thread_context *new_ctx);
extern uint64_t switch_target_pml4;



#endif