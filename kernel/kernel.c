#include "terminal.h"
#include "kprintf.h"

void _start(void) {
    terminal_init();
    terminal_set_color(VGA_WHITE, VGA_BLACK);

    terminal_clear();

    kprintf("craftyOS v0.6\n");
    kprintf("=======================\n\n");

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("VGA driver initialized\n");

    terminal_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[OK] ");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("kprintf working\n\n");

    kprintf("Format tests:\n");
    kprintf("  char:     %c\n", 'A');
    kprintf("  string:   %s\n", "hello, craftyOS");
    kprintf("  decimal:  %d\n", 12345);
    kprintf("  negative: %d\n", -42);
    kprintf("  unsigned: %u\n", 4000000000u);
    kprintf("  hex:      0x%x\n", 0xDEADBEEF);
    kprintf("  HEX:      0x%X\n", 0xCAFEBABE);
    kprintf("  percent:  100%%\n");

    kprintf("\nScroll test:\n");
    for (int i = 0; i < 20; i++) {
        kprintf("  line %d\n", i);
    }
    for (;;) {
        __asm__ volatile ("hlt");
    }
}