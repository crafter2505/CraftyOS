#include "vga.h"

#define VGA_MEMORY   ((volatile unsigned short *)0xB8000)

#define VGA_CTRL_REG     0x3D4
#define VGA_DATA_REG     0x3D5
#define VGA_CURSOR_HIGH  0x0E
#define VGA_CURSOR_LOW   0x0F

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

unsigned char vga_make_attr(unsigned char fg, unsigned char bg) {
    return (unsigned char)((bg << 4) | (fg & 0x0F));
}

void vga_init(void) {

}

void vga_set_cell(int row, int col, char c, unsigned char attr) {
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;
    }
    volatile unsigned short *vga = VGA_MEMORY;
    vga[row * VGA_WIDTH + col] = (unsigned short)(unsigned char)c | ((unsigned short)attr << 8);
}

void vga_clear_screen(unsigned char attr) {
    volatile unsigned short *vga = VGA_MEMORY;
    unsigned short blank = (unsigned short)' ' | ((unsigned short)attr << 8);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }
}

void vga_update_cursor(int row, int col) {
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;
    }
    unsigned short pos = (unsigned short)(row * VGA_WIDTH + col);

    outb(VGA_CTRL_REG, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REG, (pos >> 8) & 0xFF);

    outb(VGA_CTRL_REG, VGA_CURSOR_LOW);
    outb(VGA_DATA_REG, pos & 0xFF);
}

void vga_scroll_up(unsigned char attr) {
    volatile unsigned short *vga = VGA_MEMORY;
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }
    unsigned short blank = (unsigned short)' ' | ((unsigned short)attr << 8);
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga[i] = blank;
    }
}

void vga_hide_cursor(void) {
    outb(VGA_CTRL_REG, 0x0A);
    outb(VGA_DATA_REG, 0x20);
}