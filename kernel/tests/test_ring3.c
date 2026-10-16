#include "tests.h"
#include "userspace.h"
#include "kprintf.h"
#include "terminal.h"
void test_ring3(void) {
    terminal_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    kprintf("\n=== Ring 3 Test ===\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);

    int rv = userspace_run_embedded();
    if (rv == 0) {
        kprintf("  PASS: ring 3 executed and returned cleanly\n");
    } else {
        kprintf("  FAIL: userspace_run_embedded returned %d\n", (uint64_t)rv);
    }
}

static void test_syscall_validation(void) {
    kprintf("\n=== syscall validation test ===\n");

    uint64_t kernel_addr = 0xFFFFFFFF80000000ULL;
    uint64_t ret;

    __asm__ volatile (
        "mov $1, %%rax\n"
        "mov $1, %%rdi\n"
        "mov %1, %%rsi\n"
        "mov $16, %%rdx\n"
        "syscall\n"
        "mov %%rax, %0\n"
        : "=r"(ret)
        : "r"(kernel_addr)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );

    if (ret == (uint64_t)-1) {
        kprintf("  PASS: kernel address rejected (ret=-1)\n");
    } else {
        kprintf("  FAIL: expected -1, got %d\n", ret);
    }
}
