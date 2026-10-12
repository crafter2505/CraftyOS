/* test_wake_order.c - multiple sleepers wake in correct order */

#include "tests.h"
#include "thread.h"
#include "sched.h"
#include "timer.h"
#include "kprintf.h"

#define N_SLEEPERS 4

static volatile int wake_order[N_SLEEPERS];
static volatile int wake_index = 0;
static volatile int sleepers_done = 0;
static volatile int sleepers_ready = 0;
static volatile int release_sleepers = 0;
static volatile uint64_t sleep_epoch = 0;
static volatile uint64_t wake_deadlines[N_SLEEPERS];
static volatile uint64_t actual_wake_ticks[N_SLEEPERS];

/* Durations: 80, 40, 120, 60 ticks. Expected order: 1, 3, 0, 2. */
static const int durations[N_SLEEPERS] = { 80, 40, 120, 60 };

static void sleeper_worker(void *arg) {
    __asm__ volatile ("sti");
    int id = (int)(uint64_t)arg;
    sleepers_ready++;
    while (!release_sleepers) {
        yield();
    }

    uint64_t deadline = sleep_epoch + (uint64_t)durations[id];
    wake_deadlines[id] = deadline;
    uint64_t now = timer_get_ticks();
    if (now < deadline) {
        sched_sleep(deadline - now);
    }
    actual_wake_ticks[id] = timer_get_ticks();
    wake_order[wake_index++] = id;
    sleepers_done++;
    thread_exit();
}

void test_wake_order(void) {
    kprintf("\n=== wake order test ===\n");

    wake_index = 0;
    sleepers_done = 0;
    sleepers_ready = 0;
    release_sleepers = 0;
    for (int i = 0; i < N_SLEEPERS; i++) {
        wake_deadlines[i] = 0;
        actual_wake_ticks[i] = 0;
    }

    int created = 0;
    for (int i = 0; i < N_SLEEPERS; i++) {
        struct thread *t = thread_create("sleeper", sleeper_worker,
                                          (void *)(uint64_t)i);
        if (!t) {
            kprintf("  FAIL: unable to create sleeper %d\n", (uint64_t)i);
            continue;
        }
        sched_add(t);
        created++;
    }

    while (sleepers_ready < created) yield();
    sleep_epoch = timer_get_ticks();
    release_sleepers = 1;

    while (sleepers_done < created) yield();

    kprintf("  wake order:");
    for (int i = 0; i < wake_index; i++) {
        kprintf(" %d", (uint64_t)wake_order[i]);
    }
    kprintf("  (expected: 1 3 0 2)\n");

    for (int i = 0; i < N_SLEEPERS; i++) {
        if (wake_deadlines[i]) {
            kprintf("    thread %d: deadline=%d woke=%d\n",
                    (uint64_t)i, wake_deadlines[i], actual_wake_ticks[i]);
        }
    }

    int expected[N_SLEEPERS] = { 1, 3, 0, 2 };
    int ok = created == N_SLEEPERS && wake_index == N_SLEEPERS;
    for (int i = 0; i < N_SLEEPERS && ok; i++) {
        if (wake_order[i] != expected[i]) ok = 0;
    }

    kprintf("  %s\n", ok ? "PASS: correct wake order"
                         : "FAIL: incorrect order");
}
