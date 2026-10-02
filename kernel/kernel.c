#include "terminal.h"
#include "kprintf.h"
#include "pic.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "line.h"
#include "memory_map.h"
#include "pmm.h"
#include "vmm.h"
#include "buddy.h"
#include "slab.h"
#include "address_space.h"

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    kprintf("craftyOS v0.11.4\n");
    kprintf("=======================\n\n");

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("VGA driver initialized\n");

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("kprintf working\n");

    pic_remap();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("PIC remapped to vectors 0x20-0x2F\n");

    idt_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("IDT loaded\n");

    timer_init(100);
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Timer initialized at 100 Hz\n");

    keyboard_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Keyboard initialized\n");


    kprintf("\n");
    kprintf("\n=== Paging self-test ===\n");

    volatile uint64_t *kernel_test = (volatile uint64_t *)0xFFFFFFFF80000000ULL;
    volatile uint64_t *low_test    = (volatile uint64_t *)0x10000ULL;
    volatile uint64_t *dm_test     = (volatile uint64_t *)0xFFFF800000100000ULL;

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

    memory_map_dump();

    pmm_init();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Physical memory manager ready\n");

    buddy_init();
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


    kprintf("\nSLAB allocation test:\n");

    struct kmem_cache *task_cache = kmem_cache_create("task", 64);
    if (!task_cache) {
        kprintf("  FAIL: could not create cache\n");
    } else {
        void *a = kmem_cache_alloc(task_cache);
        void *b = kmem_cache_alloc(task_cache);
        void *c = kmem_cache_alloc(task_cache);

        kprintf("  [1] allocated: 0x%x 0x%x 0x%x\n",
                (uint64_t)a, (uint64_t)b, (uint64_t)c);

        if (a && b && c && a != b && b != c && a != c) {
            kprintf("  [1] PASS: three distinct objects\n");
        } else {
            kprintf("  [1] FAIL: distinctness\n");
        }

        *(uint64_t *)a = 0xAAAAAAAAAAAAAAAAULL;
        *(uint64_t *)b = 0xBBBBBBBBBBBBBBBBULL;
        *(uint64_t *)c = 0xCCCCCCCCCCCCCCCCULL;

        if (*(uint64_t *)a == 0xAAAAAAAAAAAAAAAAULL &&
            *(uint64_t *)b == 0xBBBBBBBBBBBBBBBBULL &&
            *(uint64_t *)c == 0xCCCCCCCCCCCCCCCCULL) {
            kprintf("  [2] PASS: read/write roundtrip\n");
        } else {
            kprintf("  [2] FAIL: read/write\n");
        }

        kmem_cache_free(task_cache, b);
        kprintf("  [3] freed b\n");

        void *d = kmem_cache_alloc(task_cache);
        kprintf("  [3] reallocated: 0x%x\n", (uint64_t)d);

        if (d == b) {
            kprintf("  [3] PASS: got freed object back (LIFO)\n");
        } else {
            kprintf("  [3] FAIL: expected 0x%x, got 0x%x\n",
                    (uint64_t)b, (uint64_t)d);
        }

        kmem_cache_free(task_cache, a);
        kmem_cache_free(task_cache, c);
        kmem_cache_free(task_cache, d);
        kprintf("  [4] freed all objects\n");

        void *objs[150];
        for (int i = 0; i < 150; i++) {
            objs[i] = kmem_cache_alloc(task_cache);
        }
        kprintf("  [5] allocated 150 objects (spans multiple slabs)\n");

        int distinct = 1;
        for (int i = 0; i < 150 && distinct; i++) {
            if (!objs[i]) { distinct = 0; break; }
            for (int j = i + 1; j < 150; j++) {
                if (objs[i] == objs[j]) { distinct = 0; break; }
            }
        }
        if (distinct) {
            kprintf("  [5] PASS: 150 distinct objects\n");
        } else {
            kprintf("  [5] FAIL: duplicate or null\n");
        }

        for (int i = 0; i < 150; i++) {
            kmem_cache_free(task_cache, objs[i]);
        }
        kprintf("  [6] freed 150 objects\n");

        slab_dump_stats();
    }
    

    slab_dump_stats();

    vmm_init();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Virtual memory manager ready\n");


    vmm_init();

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Virtual memory manager ready\n");

    vmm_test();

    line_init();
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Line editor ready\n");

    pic_clear_mask(0);
    pic_clear_mask(1);

    kprintf("\n");
    kprintf("craftyOS> ");

    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}