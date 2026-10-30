#ifndef FB_H
#define FB_H

#include <stdint.h>
#include "boot_info.h"

int fb_init(struct boot_info *bi);
uint32_t fb_width(void);
uint32_t fb_height(void);

/* Font scale: 1 = 8x16, 2 = 16x32. Cells are 8*scale wide, 16*scale tall. */
void     fb_set_font_scale(uint32_t scale);
uint32_t fb_font_scale(void);
uint32_t fb_cell_width(void);
uint32_t fb_cell_height(void);

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_clear(uint32_t color);
void fb_clear_palette(unsigned char vga_color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_cell(uint32_t x, uint32_t y, char c, unsigned char attr);

/* Blit a rectangular region of the framebuffer to another location.
 * Used for scrolling: move the whole visible text block up one cell row. */
void fb_copy_rect(uint32_t src_x, uint32_t src_y,
                  uint32_t dst_x, uint32_t dst_y,
                  uint32_t w, uint32_t h);
uint32_t vga_palette_lookup(unsigned char idx);

#endif