#include "tests.h"
#include "kprintf.h"
#include "terminal.h"

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
    test_syscall();
    test_userspace_mem();
    test_ring3();
    test_user_fault();
    test_isolation();
    test_process_lifecycle();
    test_ahci();
    test_vfs();
    test_fat32();
    test_file_syscalls();

    terminal_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("\n=== all tests completed ===\n\n");
}