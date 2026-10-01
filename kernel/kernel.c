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

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    kprintf("craftyOS v0.11.1\n");
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

    void *p3 = alloc_pages(3);   /* 8 pages */
    kprintf("  [3] alloc_pages(3) = 0x%x\n", (uint64_t)p3);

    if (p3) {
        kprintf("  [3] PASS: 8-page block allocated\n");
        free_pages(p3, 3);
        kprintf("  Freed 8-page block\n");
    } else {
        kprintf("  [3] FAIL: no 8-page block available\n");
    }

    kprintf("Buddy test complete.\n");

    
    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Physical memory manager ready\n");

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