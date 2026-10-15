#include "tests.h"
#include "kprintf.h"
#include "types.h"
#include "address_space.h"

void test_paging(void) {
    kprintf("\n=== Paging self-test ===\n");

    volatile uint64_t *kernel_test = (volatile uint64_t *)KERNEL_VIRT_BASE;
    volatile uint64_t *low_test = (volatile uint64_t *)0x10000ULL;
    volatile uint64_t *dm_test =
        (volatile uint64_t *)(DIRECT_MAP_BASE + KERNEL_PHYS_BASE);

    uint64_t val1 = *kernel_test;
    uint64_t val2 = *low_test;
    uint64_t val3 = *dm_test;

    kprintf("higher-half 0x%x -> 0x%x\n",
            (uint64_t)kernel_test, val1);
    kprintf("identity    0x0000000000010000 -> 0x%x\n", val2);
    kprintf("direct map  0x%x -> 0x%x\n",
            (uint64_t)dm_test, val3);

    if (val1 == val3) {
        kprintf("[OK] higher-half and direct map agree\n");
        kprintf("=== Self-test passed ===\n\n");
    } else {
        kprintf("[FAIL] higher-half and direct map disagree\n");
        kprintf("=== Self-test failed ===\n\n");
    }
}
