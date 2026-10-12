#include "tests.h"
#include "kprintf.h"
#include "sched.h"
#include "thread.h"

static volatile int test_waiter_ran;
static volatile int test_waker_ran;
static volatile int test_sleeper_ran;
static struct thread *g_waiter;

static void test_waiter(void *arg) {
    (void)arg;
    __asm__ volatile ("sti");
    kprintf("  [waiter] blocking...\n");
    sched_block();
    kprintf("  [waiter] resumed!\n");
    test_waiter_ran = 1;
    thread_exit();
}

static void test_waker(void *arg) {
    (void)arg;
    __asm__ volatile ("sti");
    kprintf("  [waker] sleeping 30 ticks...\n");
    sched_sleep(30);
    kprintf("  [waker] waking waiter\n");
    if (g_waiter) sched_wake(g_waiter);
    test_waker_ran = 1;
    thread_exit();
}

static void test_sleeper(void *arg) {
    (void)arg;
    __asm__ volatile ("sti");
    kprintf("  [sleeper] sleeping 10 ticks...\n");
    sched_sleep(10);
    kprintf("  [sleeper] woke after 10 ticks\n");
    test_sleeper_ran = 1;
    thread_exit();
}

void test_block(void) {
    kprintf("\n=== block/wake/sleep test ===\n");

    g_waiter = thread_create("waiter", test_waiter, 0);
    if (g_waiter) sched_add(g_waiter);

    yield();
    yield();

    struct thread *sleeper = thread_create("sleeper", test_sleeper, 0);
    if (sleeper) sched_add(sleeper);

    struct thread *waker = thread_create("waker", test_waker, 0);
    if (waker) sched_add(waker);

    while (!test_waiter_ran || !test_waker_ran || !test_sleeper_ran) {
        yield();
    }

    kprintf("  waiter=%d waker=%d sleeper=%d\n",
            (uint64_t)test_waiter_ran,
            (uint64_t)test_waker_ran,
            (uint64_t)test_sleeper_ran);

    if (test_waiter_ran && test_waker_ran && test_sleeper_ran) {
        kprintf("  PASS: block/wake/sleep all worked\n");
    } else {
        kprintf("  FAIL\n");
    }
    kprintf("=== block/wake/sleep test done ===\n\n");
}
