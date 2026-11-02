#include "tests.h"
#include "process.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"

/* Child thread: exits immediately with the given status. */
static void wait_test_child(void *arg) {
    uint32_t status = (uint32_t)(uint64_t)arg;
    process_exit(status);
    /* never returns */
}

void test_wait(void) {
    kprintf("\n=== wait test ===\n");

    int before = (int)process_count();
    kprintf("  processes before: %d\n", (uint64_t)before);

    /* [1] child exits with 42, parent waits and gets status. */
    struct process *child1 = process_create("wchild1", wait_test_child,
                                            (void *)(uint64_t)42);
    if (!child1) {
        kprintf("  FAIL: could not create child1\n");
        return;
    }
    pid_t c1_pid = child1->pid;
    kprintf("  created child pid=%d\n", (uint64_t)c1_pid);

    /* Yield until the child has run and exited. */
    for (int i = 0; i < 100; i++) {
        yield();
        if (child1->exit_called) break;
    }

    uint32_t status = 0;
    int rc = process_wait(c1_pid, &status);
    if (rc == 0 && status == 42) {
        kprintf("  [1] PASS: wait returned status 42\n");
    } else {
        kprintf("  [1] FAIL: rc=%d status=%d\n",
                (uint64_t)rc, (uint64_t)status);
        return;
    }

    /* [2] process count is back to baseline. */
    int after = (int)process_count();
    if (after == before) {
        kprintf("  [2] PASS: process count back to baseline (%d)\n",
                (uint64_t)after);
    } else {
        kprintf("  [2] FAIL: %d -> %d (leaked %d)\n",
                (uint64_t)before, (uint64_t)after,
                (uint64_t)(after - before));
    }

    /* [3] second child, different status. */
    struct process *child2 = process_create("wchild2", wait_test_child,
                                            (void *)(uint64_t)7);
    if (!child2) {
        kprintf("  [3] FAIL: could not create child2\n");
        return;
    }
    pid_t c2_pid = child2->pid;
    for (int i = 0; i < 100; i++) {
        yield();
        if (child2->exit_called) break;
    }

    uint32_t status2 = 0;
    rc = process_wait(c2_pid, &status2);
    if (rc == 0 && status2 == 7) {
        kprintf("  [3] PASS: second wait returned status 7\n");
    } else {
        kprintf("  [3] FAIL: rc=%d status=%d\n",
                (uint64_t)rc, (uint64_t)status2);
    }

    /* [4] wait on the already-reaped pid returns -1. */
    rc = process_wait(c1_pid, &status);
    if (rc == -1) {
        kprintf("  [4] PASS: wait on reaped pid returns -1\n");
    } else {
        kprintf("  [4] FAIL: wait on reaped pid returned %d\n",
                (uint64_t)rc);
    }

    kprintf("=== wait test done ===\n");
}