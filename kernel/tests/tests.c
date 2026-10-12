#include "tests.h"
#include "kprintf.h"

void run_all_tests(void) {
    test_paging();
    test_buddy();
    test_kmalloc();
    test_thread();
    test_switch();
    test_sched();
    test_block();
    test_vmm();
    test_stress();
    test_fairness();
    test_sleep_accuracy();
    test_wake_order();
    kprintf("\n=== all tests completed ===\n\n");
}