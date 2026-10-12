/* test_fairness.c - verify round-robin gives roughly equal CPU time */

#include "tests.h"
#include "thread.h"
#include "sched.h"
#include "timer.h"
#include "kprintf.h"

#define FAIRNESS_NUM_THREADS 4
#define FAIRNESS_DURATION_TICKS 300

static int fairness_worker_ids[FAIRNESS_NUM_THREADS];
static volatile uint64_t fairness_thread_ticks[FAIRNESS_NUM_THREADS];
static volatile int fairness_exited = 0;
static volatile int fairness_done = 0;

/* Each thread spins forever, incrementing its own counter. The
 * scheduler preempts it at budget exhaustion. We measure ticks. */
static void fairness_worker(void *arg) {
    __asm__ volatile ("sti");
    int idx = *(int *)arg;
    for (;;) {
        /* Busy loop. */
        for (volatile int i = 0; i < 100000; i++);
        if (fairness_done) {
            struct thread *self = thread_current();
            fairness_thread_ticks[idx] = self->total_ticks;
            fairness_exited++;
            thread_exit();
        }
    }
}

void test_fairness(void) {
    kprintf("\n=== fairness test ===\n");
    kprintf("  creating %d CPU-bound threads for %d ticks...\n",
            (uint64_t)FAIRNESS_NUM_THREADS,
            (uint64_t)FAIRNESS_DURATION_TICKS);

    fairness_done = 0;
    fairness_exited = 0;
    for (int i = 0; i < FAIRNESS_NUM_THREADS; i++) {
        fairness_thread_ticks[i] = 0;
    }

    int created = 0;
    for (int i = 0; i < FAIRNESS_NUM_THREADS; i++) {
        fairness_worker_ids[i] = i;
        struct thread *t = thread_create("fair", fairness_worker, &fairness_worker_ids[i]);
        if (!t) {
            kprintf("  FAIL: unable to create fairness thread %d\n", (uint64_t)i);
            break;
        }
        sched_add(t);
        created++;
    }

    if (created != FAIRNESS_NUM_THREADS) {
        fairness_done = 1;
        while (fairness_exited < created) {
            yield();
        }
        return;
    }

    /* Wait the target duration. */
    uint64_t start = timer_get_ticks();
    while (timer_get_ticks() - start < FAIRNESS_DURATION_TICKS) {
        yield();
    }

    fairness_done = 1;
    while (fairness_exited < created) {
        yield();
    }

    /* Measure tick distribution. */
    uint64_t total_ticks = 0;
    uint64_t min_ticks = (uint64_t)-1;
    uint64_t max_ticks = 0;

    for (int i = 0; i < FAIRNESS_NUM_THREADS; i++) {
        uint64_t t = fairness_thread_ticks[i];
        total_ticks += t;
        if (t < min_ticks) min_ticks = t;
        if (t > max_ticks) max_ticks = t;
        kprintf("    thread %d: %d ticks\n", (uint64_t)i, t);
    }

    uint64_t ideal = total_ticks / FAIRNESS_NUM_THREADS;
    kprintf("  total=%d ideal=%d min=%d max=%d\n",
            total_ticks, ideal, min_ticks, max_ticks);

    /* Round-robin should be within ~30% of ideal for every thread. */
    uint64_t tolerance = ideal / 3;
    int fair = 1;
    for (int i = 0; i < FAIRNESS_NUM_THREADS; i++) {
        uint64_t t = fairness_thread_ticks[i];
        if (t + tolerance < ideal || t > ideal + tolerance) {
            fair = 0;
        }
    }

    if (fair) {
        kprintf("  PASS: distribution within tolerance\n");
    } else {
        kprintf("  FAIL: uneven distribution\n");
    }
}