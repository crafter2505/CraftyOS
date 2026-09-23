#include "terminal.h"
#include "kprintf.h"
#include "pic.h"
#include "idt.h"

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_clear();

    kprintf("craftyOS v0.7\n");
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

    kprintf("\nTesting interrupt handling with int 3 (breakpoint)...\n");
    __asm__ volatile ("int $3");

    kprintf("Returned from int 3 — dispatcher works!\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}