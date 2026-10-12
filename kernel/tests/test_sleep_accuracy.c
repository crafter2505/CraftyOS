/* test_sleep_accuracy.c - verify sleep timing */

#include "tests.h"
#include "thread.h"
#include "sched.h"
#include "timer.h"
#include "kprintf.h"

#define SLEEP_TARGET 50
#define SLEEP_TOLERANCE 5

static volatile int sleep_done = 0;
static volatile uint64_t sleep_start = 0;
static volatile uint64_t sleep_end = 0;

static void sleep_worker(void *arg) {
    (void)arg;
    __asm__ volatile ("sti");
    sleep_start = timer_get_ticks();
    sched_sleep(SLEEP_TARGET);
    sleep_end = timer_get_ticks();
    sleep_done = 1;
    thread_exit();
}

void test_sleep_accuracy(void) {
    kprintf("\n=== sleep accuracy test ===\n");

    sleep_done = 0;
    sleep_start = 0;
    sleep_end = 0;
    struct thread *t = thread_create("sleeper", sleep_worker, 0);
    if (!t) {
        kprintf("  FAIL: unable to create sleeper thread\n");
        return;
    }
    sched_add(t);

    while (!sleep_done) yield();

    uint64_t elapsed = sleep_end - sleep_start;
    kprintf("  slept for %d ticks (target %d)\n",
            elapsed, (uint64_t)SLEEP_TARGET);

    uint64_t diff = (elapsed > SLEEP_TARGET)
                  ? elapsed - SLEEP_TARGET
                  : SLEEP_TARGET - elapsed;

    if (diff <= SLEEP_TOLERANCE) {
        kprintf("  PASS: within %d ticks\n", (uint64_t)SLEEP_TOLERANCE);
    } else {
        kprintf("  FAIL: off by %d ticks\n", diff);
    }
}
