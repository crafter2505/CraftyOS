#include "terminal.h"
#include "kprintf.h"
#include "pic.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "line.h"
#include "memory_map.h"
#include "pmm.h"

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    kprintf("craftyOS v0.8.3\n");
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

    memory_map_dump();

    pmm_init();
    kprintf("\nPMM test:\n");
    kprintf("  Free pages before: %d\n", (long)pmm_get_free_pages());

    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    void *p3 = pmm_alloc_page();

    kprintf("  Allocated: 0x%x, 0x%x, 0x%x\n",
            (uint64_t)p1, (uint64_t)p2, (uint64_t)p3);
    kprintf("  Free pages after alloc: %d\n", (long)pmm_get_free_pages());

    pmm_free_page(p2);
    kprintf("  Freed middle page\n");
    kprintf("  Free pages after free: %d\n", (long)pmm_get_free_pages());

    void *p4 = pmm_alloc_page();
    kprintf("  Allocated again: 0x%x (should match 0x%x)\n",
            (uint64_t)p4, (uint64_t)p2);

    if (p4 == p2) {
        kprintf("  PASS: reused freed page\n");
    } else {
        kprintf("  FAIL: allocated a different page\n");
    }

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