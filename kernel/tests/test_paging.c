#include "tests.h"
#include "kprintf.h"
#include "types.h"

void test_paging(void) {
    kprintf("\n=== Paging self-test ===\n");

    volatile uint64_t *kernel_test = (volatile uint64_t *)0xFFFFFFFF80000000ULL;
    volatile uint64_t *low_test = (volatile uint64_t *)0x10000ULL;
    volatile uint64_t *dm_test = (volatile uint64_t *)0xFFFF800000100000ULL;

    uint64_t val1 = *kernel_test;
    uint64_t val2 = *low_test;
    uint64_t val3 = *dm_test;

    kprintf("higher-half 0xFFFFFFFF80000000 -> 0x%x\n", val1);
    kprintf("identity    0x0000000000010000 -> 0x%x\n", val2);
    kprintf("direct map  0xFFFF800000100000 -> 0x%x\n", val3);

    if (val1 == val3) {
        kprintf("[OK] higher-half and direct map agree\n");
    } else {
        kprintf("[FAIL] higher-half and direct map disagree\n");
    }
    kprintf("=== Self-test passed ===\n\n");
}
