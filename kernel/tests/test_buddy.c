#include "tests.h"
#include "buddy.h"
#include "address_space.h"
#include "kprintf.h"
#include "memory_map.h"

static void run_buddy_tests(void) {
    buddy_dump_stats();

    kprintf("\nBuddy test:\n");

    void *p1 = alloc_pages(0);
    kprintf("  [1] alloc_pages(0) = 0x%x\n", (uint64_t)p1);

    void *p2 = alloc_pages(0);
    kprintf("  [2] alloc_pages(0) = 0x%x\n", (uint64_t)p2);

    if (p1 && p2 && p1 != p2) {
        kprintf("  [1,2] PASS: two distinct pages\n");
    } else {
        kprintf("  [1,2] FAIL\n");
    }

    free_pages(p2, 0);
    free_pages(p1, 0);
    kprintf("  Freed both pages\n");

    void *p3 = alloc_pages(3);
    kprintf("  [3] alloc_pages(3) = 0x%x\n", (uint64_t)p3);

    if (p3) {
        kprintf("  [3] PASS: 8-page block allocated\n");
        free_pages(p3, 3);
        kprintf("  Freed 8-page block\n");
    } else {
        kprintf("  [3] FAIL: no 8-page block available\n");
    }

    kprintf("Buddy test complete.\n");

    kprintf("\nBuddy alloc diagnostic:\n");
    for (int i = 0; i < 10; i++) {
        void *p = alloc_pages(0);
        kprintf("  [%d] phys=0x%x virt=0x%x\n",
                i, (uint64_t)p,
                (uint64_t)p + DIRECT_MAP_BASE);
        free_pages(p, 0);
    }
    kprintf("Stack region: 0x80000-0x90000 (RSP lives here)\n");
    kprintf("Kernel: 0x200000-0x5AE000\n");
    kprintf("Page tables: 0x10000-0x17000\n");
    kprintf("Stage 1/2: 0x7C00-0x10000\n");
}

void test_buddy(void) {
    run_buddy_tests();
}
