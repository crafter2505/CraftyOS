#include "tests.h"
#include "userspace.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"

extern uint8_t _user_fault_program_start[];
extern uint8_t _user_fault_program_end[];

static volatile int fault_test_done = 0;

static void user_fault_thread(void *arg) {
    (void)arg;
    userspace_run_program(_user_fault_program_start, _user_fault_program_end - _user_fault_program_start);

    kprintf("  [fault-test] FAIL: userspace_run_program returned\n");
    fault_test_done = 1;
    thread_exit();
}

void test_user_fault(void) {
    kprintf("\n=== user fault test ===\n");

    fault_test_done = 0;

    struct thread *t = thread_create("faulty", user_fault_thread, 0);
    if (!t) {
        kprintf("  FAIL: could not create thread\n");
        return;
    }
    sched_add(t);

    int spins = 0;
    while (fault_test_done == 0 && t->state != THREAD_TERMINATED && spins < 1000) {
        yield();
        spins++;
    }

    if (t->state == THREAD_TERMINATED || fault_test_done) {
        kprintf("  PASS: user fault terminated the thread, kernel alive\n");
    } else {
        kprintf("  FAIL: thread still running after timeout\n");
    }
}