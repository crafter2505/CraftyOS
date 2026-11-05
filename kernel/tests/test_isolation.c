#include "tests.h"
#include "userspace.h"
#include "process.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"

static void iso_thread_a(void *arg) {
    (void)arg;
    (void)userspace_run_embedded();
}

static void iso_thread_b(void *arg) {
    (void)arg;
    (void)userspace_run_embedded();
}

void test_isolation(void) {
    kprintf("\n=== isolation test ===\n");

    struct process *pa = process_create("procA", iso_thread_a, 0);
    struct process *pb = process_create("procB", iso_thread_b, 0);

    if (!pa || !pb) {
        kprintf("  FAIL: process creation\n");
        return;
    }

    int spins = 0;
    while (spins < 500 &&
           (pa->threads->state != THREAD_TERMINATED ||
            pb->threads->state != THREAD_TERMINATED)) {
        yield();
        spins++;
    }

    if (pa->pml4_phys != pb->pml4_phys &&
        pa->pml4_phys != 0x10000 &&
        pb->pml4_phys != 0x10000) {
        kprintf("  PASS: two distinct address spaces\n");
    } else {
        kprintf("  FAIL: address spaces not distinct\n");
        return;
    }

    if (!pa->last_code_phys || !pb->last_code_phys ||
        pa->last_code_phys == pb->last_code_phys) {
        kprintf("  FAIL: user code pages are missing or shared\n");
        return;
    }
    kprintf("  PASS: user code mapped to distinct physical pages\n");
    kprintf("        procA: phys 0x%x\n", pa->last_code_phys);
    kprintf("        procB: phys 0x%x\n", pb->last_code_phys);

    if (pa->threads->state == THREAD_TERMINATED &&
        pb->threads->state == THREAD_TERMINATED) {
        kprintf("  PASS: both processes ran and exited cleanly\n");
    } else {
        kprintf("  FAIL: process did not terminate\n");
    }

    kprintf("  PASS: isolation verified\n");
}