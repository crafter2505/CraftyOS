#include "tests.h"
#include "userspace.h"
#include "thread.h"
#include "sched.h"
#include "kprintf.h"

extern uint8_t _user_file_program_start[];
extern uint8_t _user_file_program_end[];

static volatile int file_test_done = 0;
static volatile int file_test_status = -1;

static void file_test_thread(void *arg) {
    (void)arg;

    int ret = userspace_run_program(
        _user_file_program_start,
        _user_file_program_end - _user_file_program_start);

    /* userspace_run_program returns 0 on clean exit, -1 on failure.
     * The actual exit status is captured in the process's exit_status. */
    file_test_status = ret;
    file_test_done = 1;
    thread_exit();
}

void test_file_syscalls(void) {
    kprintf("\n=== file syscalls test ===\n");

    file_test_done = 0;
    file_test_status = -1;

    struct thread *t = thread_create("filetest", file_test_thread, 0);
    if (!t) {
        kprintf("  FAIL: could not create thread\n");
        return;
    }
    sched_add(t);

    int spins = 0;
    while (!file_test_done && spins < 1000) {
        yield();
        spins++;
    }

    if (file_test_done && file_test_status == 0) {
        kprintf("  PASS: user opened and closed /mnt/efi/boot/kernel.elf\n");
    } else {
        kprintf("  FAIL: user program exit status %d\n",
                (uint64_t)file_test_status);
    }
}