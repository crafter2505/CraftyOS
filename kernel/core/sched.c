#include "sched.h"
#include "thread.h"
#include "process.h"
#include "kmalloc.h"
#include "kprintf.h"
#include "timer.h"
#include "percpu.h"
#include "vmm.h"

static struct thread *idle_thread = 0;

void sched_init(void) {
    idle_thread = thread_create("idle", 0, 0);
    if (!idle_thread) {
        kprintf(" SCHED: failed to create idle thread\n");
        return;
    }

    idle_thread->process = process_current();
    sched_add(idle_thread);
    kprintf(" SCHED: initialized (idle thread=%d)\n", (uint64_t)idle_thread->id);
}

void sched_add(struct thread *t) {
    if (!t) return;
    if (t->next) return;

    struct thread *cur = thread_current();

    if (!cur) {
        t->next = t;
        return;
    }
    t->next = cur->next ? cur->next : cur;
    cur->next = t;
}
void sched_remove(struct thread *t) {
    if (!t) return;

    struct thread *cur = thread_current();

    if (t == cur) {
        t->state = THREAD_TERMINATED;
        return;
    }

    struct thread *prev = cur;
    while (prev && prev->next != t) {
        prev = prev->next;
        if (prev == cur) {
            return;
        }
    }

    if (prev && prev->next == t) {
        prev->next = t->next;
        t->next = 0;
    }
}

static struct thread *find_next_runnable(struct thread *start) {
    struct thread *first = start->next ? start->next : start;
    struct thread *t = first;

    while (t != start) {
        if (t->state == THREAD_READY || t->state == THREAD_RUNNING) {
            return t;
        }
        t = t->next ? t->next : start;
        if (t == first) break;
    }

    if (idle_thread && idle_thread != start &&
        (idle_thread->state == THREAD_READY ||
         idle_thread->state == THREAD_RUNNING)) {
        return idle_thread;
    }

    return start;
}

void sched_unlink_current(void) {
    struct thread *self = thread_current();
    if (!self || !self->next || self->next == self) return;

    struct thread *prev = self;
    while (prev->next != self) {
        prev = prev->next;
        if (!prev || prev == self) return;
    }

    prev->next = self->next;
}

void schedule(void) {
    struct thread *cur = thread_current();
    if (!cur) return;

    struct thread *next = find_next_runnable(cur);
    if (next == cur) return;

    if (cur->state == THREAD_RUNNING) cur->state = THREAD_READY;
    next->state = THREAD_RUNNING;

    uint64_t kernel_stack = next->syscall_stack
                          ? (uint64_t)next->syscall_stack +
                                next->syscall_stack_size
                          : 0x90000;
    percpu_set_kernel_stack(kernel_stack);

    if (next->process) {
        vmm_switch_address_space(next->process->pml4_phys);
    }

    set_current_thread(next);
    switch_to(&cur->ctx, &next->ctx);
}

void sched_block(void) {
    struct thread *cur = thread_current();
    if (!cur) return;
    cur->state = THREAD_BLOCKED;
    schedule();
}

void shced_wake(struct thread *t) {
    if (!t) return;
    if (t->state == THREAD_BLOCKED || t-> state == THREAD_SLEEPING) {
        t->state = THREAD_READY;
        t->wake_at_tick = 0;
    }
}

void sched_sleep(uint64_t ticks) {
    struct thread *cur = thread_current();
    if (!cur) return;
    if (ticks == 0) {
        schedule();
        return;
    }

    cur->wake_at_tick = timer_get_ticks() + ticks;
    cur->state = THREAD_SLEEPING;
    schedule();
}

void sched_wake(struct thread *t) {
    if (!t) return;

    if (t->state == THREAD_BLOCKED || t->state == THREAD_SLEEPING) {
        t->state = THREAD_READY;
        t->wake_at_tick = 0;
    }
}

void sched_check_sleepers(void) {
    struct thread *cur = thread_current();
    if (!cur) return;
    uint64_t now = timer_get_ticks();
    struct thread *t = cur->next;
    while (t && t != cur) {
        if (t->state == THREAD_SLEEPING && t->wake_at_tick <= now) {
            t->state = THREAD_READY;
            t->wake_at_tick = 0;
        }
        t = t->next;
    }
    if (cur->state == THREAD_SLEEPING && cur->wake_at_tick <= now) {
        cur->state = THREAD_READY;
        cur->wake_at_tick = 0;
    }
}

void sched_tick(void) {
    struct thread *cur = thread_current();
    if (!cur) return;
    sched_check_sleepers();
    cur->total_ticks++;
    if (cur->budget > 0) cur->budget--;
    if (cur->budget == 0) {
        cur->budget = TIME_SLICE_TICKS;
        uint64_t flags;
        __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        schedule();
        if (flags & (1ULL << 9)) {
            __asm__ volatile ("sti" : : : "memory");
        }
    }
}

void sched_idle_loop(void) {
    for (;;) {
        thread_reap_pending();
        __asm__ volatile ("sti; hlt");
    }
}

void yield(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    schedule();
    if (flags & (1ULL << 9)) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

void sched_dump(void) {
    struct thread *cur = thread_current();
    if (!cur) {
        kprintf("  SCHED: no current thread\n");
        return;
    }

    kprintf("  SCHED run queue:\n");
    struct thread *t = cur;
    int count = 0;
    do {
        kprintf("    [%d] '%s' id=%d state=%d budget=%d ticks=%d\n",
                count++,
                t->name,
                (uint64_t)t->id,
                (uint64_t)t->state,
                (uint64_t)t->budget,
                t->total_ticks);
        t = t->next ? t->next : cur;
    } while (t != cur && count < 20);

    kprintf("  SCHED: %d threads in queue\n", (uint64_t)count);
}
