#include "tests.h"
#include "kmalloc.h"
#include "kprintf.h"
#include "page.h"
#include "slab.h"
#include "terminal.h"

static void run_kmalloc_tests(void) {
    kprintf("\nkmalloc test:\n");

    void *a = kmalloc(100);
    kprintf("  [1] kmalloc(100) = 0x%x\n", (uint64_t)a);
    if (a) {
        *(uint64_t *)a = 0xAAAAAAAAAAAAAAAAULL;
        if (*(uint64_t *)a == 0xAAAAAAAAAAAAAAAAULL) {
            terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            kprintf("  [1] PASS: write/read 8 bytes\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
        } else {
            terminal_set_color(VGA_RED, VGA_BLACK);
            kprintf("  [1] FAIL: read/write\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
            }
    } else {
        terminal_set_color(VGA_RED, VGA_BLACK);
        kprintf("  [1] FAIL: allocation failed\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }

    void *b = kzalloc(256);
    kprintf("  [2] kzalloc(256) = 0x%x\n", (uint64_t)b);
    if (b) {
        int all_zero = 1;
        for (int i = 0; i < 256; i++) {
            if (((uint8_t *)b)[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero) { 
            terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            kprintf("  [2] PASS: zeroed\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
        }
        else {
            terminal_set_color(VGA_RED, VGA_BLACK);
            kprintf("  [2] FAIL: not zeroed\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
        }
    } else {
        terminal_set_color(VGA_RED, VGA_BLACK);
        kprintf("  [2] FAIL: allocation failed\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }

    void *c = kmalloc(5000);
    kprintf("  [3] kmalloc(5000) = 0x%x\n", (uint64_t)c);
    if (c) {
        *(uint64_t *)c = 0xCCCCCCCCCCCCCCCCULL;
        if (*(uint64_t *)c == 0xCCCCCCCCCCCCCCCCULL) {
            terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            kprintf("  [3] PASS: large alloc write/read\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
        } else {
            terminal_set_color(VGA_RED, VGA_BLACK);
            kprintf("  [3] FAIL\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
        }
    } else {
        terminal_set_color(VGA_RED, VGA_BLACK);
        kprintf("  [3] FAIL: allocation failed\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }

    if (a) {
        struct page *pa = page_from_virt((uint64_t)a);
        if (pa) {
            kprintf("  [4] page metadata for a:\n");
            kprintf("      flags = 0x%x (expect PAGE_FLAG_SLAB=0x%x)\n",
                    pa->flags, PAGE_FLAG_SLAB);
            kprintf("      cache = 0x%x\n", (uint64_t)pa->cache);
            if (pa->flags == PAGE_FLAG_SLAB && pa->cache != 0) {
                kprintf("      cache name = %s\n", pa->cache->name);
                terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
                kprintf("  [4] PASS: page metadata populated\n");
                terminal_set_color(VGA_WHITE, VGA_BLACK);
            } else {
                terminal_set_color(VGA_RED, VGA_BLACK);
                kprintf("  [4] FAIL: metadata wrong\n");
                terminal_set_color(VGA_WHITE, VGA_BLACK);
            }
        } else {
            terminal_set_color(VGA_RED, VGA_BLACK);
            kprintf("  [4] FAIL: page_from_virt returned NULL\n");
            terminal_set_color(VGA_WHITE, VGA_BLACK);
        }
    }

    if (c) {
        struct page *pc = page_from_virt((uint64_t)c);
        if (pc) {
            kprintf("  [5] page metadata for c (buddy):\n");
            kprintf("      flags = 0x%x (expect PAGE_FLAG_BUDDY_ALLOC=0x%x)\n",
                    pc->flags, PAGE_FLAG_BUDDY_ALLOC);
            kprintf("      order = %d (5000 bytes -> 2 pages -> order=1)\n",
                    pc->order);
            if (pc->flags == PAGE_FLAG_BUDDY_ALLOC && pc->order == 1) {
                terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
                kprintf("  [5] PASS: buddy metadata correct\n");
                terminal_set_color(VGA_WHITE, VGA_BLACK);
            } else {
                terminal_set_color(VGA_RED, VGA_BLACK);
                kprintf("  [5] FAIL: buddy metadata wrong\n");
                terminal_set_color(VGA_WHITE, VGA_BLACK);
            }
        }
    }

    void *ptrs[200];
    int ok = 1;
    for (int i = 0; i < 200; i++) {
        ptrs[i] = 0;
    }
    for (int i = 0; i < 200; i++) {
        ptrs[i] = kmalloc(32 + (i % 4) * 16);
        if (!ptrs[i]) {
            ok = 0;
            break;
        }
    }
    if (ok) {
        terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kprintf("  [6] PASS: 200 varied allocations\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }
    else {
        terminal_set_color(VGA_RED, VGA_BLACK);
        kprintf("  [6] FAIL\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }
    for (int i = 0; i < 200; i++) {
        if (ptrs[i]) kfree(ptrs[i]);
    }
    kprintf("  [7] Freed all\n");

    kfree(a);
    kfree(b);
    kfree(c);
    kmalloc_dump_stats();
}

void test_kmalloc(void) {
    run_kmalloc_tests();
}
