#include "thread.h"
#include "process.h"
#include "kprintf.h"
#include "kmalloc.h"

static uint64_t next_thread_id = 1;
static uint64_t total_threads_created = 0;

static struct thread * current_thread = 0;

void thread_init_subsystem(void) {
    struct thread *boot = (struct thread *)kmalloc(sizeof(struct thread));
    if (!boot) {
        kprintf(" THREAD: failed to allocate boot thread\n");
        return;
    }
    uint8_t *p = (uint8_t *)boot;
    for (unsigned long i = 0; i < sizeof(struct thread); i++) p[i] = 0;

    boot->id = next_thread_id++;
    const char *bn = "boot";
    int i = 0;
    while (bn[i] && i < THREAD_NAME_LEN -1) {boot->name[i] = bn[i]; i++;}
    boot->name[i] = 0;
    boot->state = THREAD_RUNNING;
    boot->stack_base = 0;
    boot->stack_size = 0;
    boot->process = 0;
    boot->next = 0;
    boot->total_ticks = 0;
    current_thread = boot;
    total_threads_created++;

    kprintf(" THREAD: boot thread id=%d created\n", (uint64_t)boot->id);
}

static void (*pending_entry)(void) = 0;

void thread_set_pending_entry(void (*entry)(void)) {
    pending_entry = entry;
}
void thread_trampoline(void) {
    __asm__ volatile ("sti");
    if (pending_entry) {
        pending_entry();
    }
    thread_exit();
}

struct thread *thread_create(const char *name, void (*entry)(void)) {
    struct thread *t = (struct thread *)kmalloc(sizeof(struct thread));
    if (!t) return 0;

    uint8_t *p = (uint8_t *)t;
    for (unsigned long i = 0; i < sizeof(struct thread); i++) p[i] = 0;

    void *stack = kmalloc(THREAD_STACK_SIZE);
    if (!stack) {
        kfree(t);
        return 0;
    }

    t->id = next_thread_id++;
    int i = 0;
    while (name[i] && i < THREAD_NAME_LEN - 1) { t->name[i] = name[i]; i++; }
    t->name[i] = 0;

    t->state = THREAD_READY;
    t->stack_base = stack;
    t->stack_size = THREAD_STACK_SIZE;
    t->process = 0;
    t->next = 0;
    t->total_ticks = 0;

    uint64_t stack_top = (uint64_t)stack + THREAD_STACK_SIZE;

    stack_top &= ~0xFULL;

    t->ctx.rsp    = stack_top;
    t->ctx.rip    = (uint64_t)thread_trampoline;
    t->ctx.rflags = 0x202;


    total_threads_created++;

    kprintf("  THREAD: created '%s' id=%d stack=0x%x\n",
            t->name, (uint64_t)t->id, (uint64_t)stack);

    return t;
}

void thread_exit(void) {
    struct thread *t = current_thread;
    if (t) {
        t->state = THREAD_TERMINATED;
        kprintf("  THREAD: '%s' (id=%d) exiting\n", t->name, (uint64_t)t->id);
    }

    /* TODO session 12.4: schedule() instead of halting */
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

struct thread *thread_current(void) {
    return current_thread;
}

uint64_t thread_count_total(void) {
    return total_threads_created;
}

void thread_dump_all(void) {
    kprintf("  THREAD: %d threads created total\n",
            total_threads_created);
    if (current_thread) {
        kprintf("    current: '%s' id=%d state=%d\n",
                current_thread->name,
                (uint64_t)current_thread->id,
                (uint64_t)current_thread->state);
    }
}