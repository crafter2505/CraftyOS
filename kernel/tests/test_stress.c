#include "tests.h"
#include "thread.h"
#include "sched.h"
#include "kmalloc.h"
#include "pmm.h"
#include "kprintf.h"

static volatile int stress_counter = 0;
static void stress_worker(void *arg) {
    (void)arg;
    __asm__ volatile ("sti");
    stress_counter++;
    thread_exit();
}

void test_stress(void) {
    kprintf("\n=== stress test ===\n");

    uint64_t free_before = pmm_get_free_pages();
    kprintf("  free pages before: %d\n", (long)free_before);

    int failed = 0;
    for (int batch = 0; batch < 10; batch++) {
        stress_counter = 0;
        int created = 0;

        for (int i = 0; i < 10; i++) {
            struct thread *t = thread_create("stress", stress_worker, 0);
            if (!t) {
                kprintf("  FAIL: unable to create thread in batch %d\n",
                        (uint64_t)batch);
                failed = 1;
                continue;
            }
            sched_add(t);
            created++;
        }

        while (stress_counter < created) yield();
        if (created != 10) failed = 1;

        for (int i = 0; i < 20; i++) {
            yield();
        }
    }

    for (int i = 0; i < 50; i++) {
        yield();
    }

    uint64_t free_after = pmm_get_free_pages();
    kprintf("  free pages after:  %d\n", (long)free_after);

    if (!failed && free_after >= free_before - 4) {
        kprintf("  PASS: no significant leak\n");
    } else {
        kprintf("  FAIL: leaked %d pages\n",
                (long)(free_before - free_after));
    }
}