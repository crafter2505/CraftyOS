#include "tests.h"
#include "process.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"

static volatile int lifecycle_passed;

static void child_thread(void *arg) {
    (void)arg;
    struct process *child = process_current();
    kprintf("  [child] pid=%d, running\n",
            child ? (uint64_t)child->pid : 0);
    sched_sleep(5);
    kprintf("  [child] calling process_exit(42)\n");
    process_exit(42);
}

static void parent_thread(void *arg) {
    (void)arg;
    kprintf("  [parent] creating child\n");
    struct process *child = process_create("child", child_thread, 0);
    if (!child) {
        kprintf("  FAIL: process_create\n");
        return;
    }

    sched_add(child->threads);
    kprintf("  [parent] child pid=%d, waiting...\n", (uint64_t)child->pid);

    uint32_t status = 0;
    if (process_wait(child->pid, &status) != 0) {
        kprintf("  FAIL: process_wait returned error\n");
        return;
    }

    kprintf("  [parent] child exited with status %d\n", (uint64_t)status);
    if (status == 42) {
        kprintf("  PASS: exit status 42 received\n");
        lifecycle_passed = 1;
    } else {
        kprintf("  FAIL: expected 42, got %d\n", (uint64_t)status);
    }
}

void test_process_lifecycle(void) {
    kprintf("\n=== process lifecycle test ===\n");
    lifecycle_passed = 0;
    parent_thread(0);
    if (!lifecycle_passed) {
        kprintf("  FAIL: process lifecycle test\n");
    }
    kprintf("=== process lifecycle test done ===\n");
}
