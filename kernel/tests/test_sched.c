#include "tests.h"
#include "kprintf.h"
#include "pmm.h"
#include "sched.h"
#include "thread.h"

static volatile int reap_test_counter;

static void reap_test_thread(void) {
    __asm__ volatile ("sti");
    reap_test_counter++;
    thread_exit();
}

void test_sched(void) {
    kprintf("\n=== reap test ===\n");

    uint64_t free_before = pmm_get_free_pages();
    kprintf("  free pages before: %d\n", (long)free_before);

    int failed = 0;
    for (int i = 0; i < 10; i++) {
        struct thread *t = thread_create("worker", reap_test_thread);
        if (!t) {
            kprintf("  FAIL: unable to create worker thread\n");
            failed = 1;
            break;
        }
        sched_add(t);

        int expected = i + 1;
        while (reap_test_counter < expected) {
            yield();
        }
    }

    kprintf("  created and completed %d threads\n",
            (uint64_t)reap_test_counter);

    for (int i = 0; i < 5; i++) {
        yield();
    }

    uint64_t free_after = pmm_get_free_pages();
    kprintf("  free pages after:  %d\n", (long)free_after);

    if (!failed && free_after >= free_before - 2) {
        kprintf("  PASS: memory returned to baseline\n");
    } else {
        kprintf("  FAIL: leaked %d pages\n",
                (long)(free_before - free_after));
    }
    kprintf("=== reap test done ===\n\n");
}
