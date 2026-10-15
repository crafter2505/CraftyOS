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
