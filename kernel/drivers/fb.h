#ifndef FB_H
#define FB_H

#include "types.h"
#include "boot_info.h"


int  fb_init(struct boot_info *bi);

uint32_t fb_width(void);
uint32_t fb_height(void);

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);


void fb_clear(uint32_t color);

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

#endif