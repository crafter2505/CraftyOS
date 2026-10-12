#ifndef VGA_H
#define VGA_H

#define VGA_BLACK         0
#define VGA_BLUE          1
#define VGA_GREEN         2
#define VGA_CYAN          3
#define VGA_RED           4
#define VGA_MAGENTA       5
#define VGA_BROWN         6
#define VGA_LIGHT_GREY    7
#define VGA_DARK_GREY     8
#define VGA_LIGHT_BLUE    9
#define VGA_LIGHT_GREEN   10
#define VGA_LIGHT_CYAN    11
#define VGA_LIGHT_RED     12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW        14
#define VGA_WHITE         15

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
void vga_init(void);
void vga_set_cell(int row, int col, char c, unsigned char attr);
void vga_clear_screen(unsigned char attr);
void vga_update_cursor(int row, int col);
void vga_hide_cursor(void);

unsigned char vga_make_attr(unsigned char fg, unsigned char bg);

#endif