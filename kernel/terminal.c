#include "terminal.h"
#include "vga.h"

static int cursor_row = 0;
static int cursor_col = 0;
static unsigned char current_attr = 0x07;

void terminal_init(void) {
    vga_init();
    cursor_row = 0;
    cursor_col = 0;
    current_attr = vga_make_attr(VGA_LIGHT_GREY, VGA_BLACK);
}

static void terminal_scroll(void) {
    for (int row = 0; row < VGA_HEIGHT - 1; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            volatile unsigned short *vga = (volatile unsigned short *)0xB8000;
            vga[row * VGA_WIDTH + col] = vga[(row + 1) * VGA_WIDTH + col];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++) {
        vga_set_cell(VGA_HEIGHT - 1, col, ' ', current_attr);
    }

    cursor_row = VGA_HEIGHT - 1;
}

void terminal_clear(void) {
    vga_clear_screen(current_attr);
    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor(cursor_row, cursor_col);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 4) & ~3;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    } else {
        vga_set_cell(cursor_row, cursor_col, c, current_attr);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        terminal_scroll();
    }

    vga_update_cursor(cursor_row, cursor_col);
}

void terminal_puts(const char *s) {
    while (*s) {
        terminal_putchar(*s++);
    }
}

void terminal_set_color(unsigned char fg, unsigned char bg) {
    current_attr = vga_make_attr(fg, bg);
}