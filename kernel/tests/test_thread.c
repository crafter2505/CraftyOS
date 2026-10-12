#include "tests.h"
#include "kprintf.h"
#include "address_space.h"
#include "process.h"
#include "sched.h"
#include "thread.h"
#include "memory_map.h"

static int test_counter;
static volatile int test_finished;

static void test_thread_entry(void *arg) {
    (void)arg;
    for (int i = 0; i < 5; i++) {
        test_counter++;
        kprintf("  [test] counter=%d\n", (uint64_t)test_counter);
        for (volatile int j = 0; j < 5000000; j++) {
        }
        yield();
    }
    kprintf("  [test] done\n");
    test_finished = 1;
}

void test_thread(void) {
    kprintf("\n=== Thread/process subsystem ===\n");
    thread_dump_all();
    process_dump_all();

    struct thread *test_thread = thread_create("test", test_thread_entry, 0);
    if (test_thread) {
        kprintf("  TEST: created thread id=%d state=%d\n",
                (uint64_t)test_thread->id,
                (uint64_t)test_thread->state);
        kprintf("  TEST: ctx.rip=0x%x ctx.rsp=0x%x ctx.rflags=0x%x\n",
                test_thread->ctx.rip,
                test_thread->ctx.rsp,
                test_thread->ctx.rflags);

        if (test_thread->ctx.rip != 0 &&
            test_thread->ctx.rsp != 0 &&
            test_thread->state == THREAD_READY) {
            kprintf("  [1] PASS: thread created with valid context\n");
        } else {
            kprintf("  [1] FAIL: thread context invalid\n");
        }
    } else {
        kprintf("  [1] FAIL: thread_create returned NULL\n");
    }

    if (test_thread) {
        uint64_t stack_addr = (uint64_t)test_thread->stack_base;
        if (stack_addr >= DIRECT_MAP_BASE &&
            stack_addr < DIRECT_MAP_BASE + 0x8000000ULL) {
            kprintf("  [2] PASS: stack in direct-map region\n");
        } else {
            kprintf("  [2] FAIL: stack at 0x%x (unexpected)\n", stack_addr);
        }
        sched_add(test_thread);
        while (!test_finished) {
            yield();
        }
    }

    thread_dump_all();
}
