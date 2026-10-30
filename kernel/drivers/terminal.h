#ifndef TERMINAL_H
#define TERMINAL_H

#include "vga.h"

void terminal_init(void);
void terminal_clear(void);
void terminal_putchar(char c);
void terminal_puts(const char *s);
void terminal_set_color(unsigned char fg, unsigned char bg);

void terminal_scrollback_up(int lines);
void terminal_scrollback_down(int lines);
void terminal_scrollback_top(void);
void terminal_scrollback_bottom(void);
int  terminal_is_scrolled_back(void);
void terminal_redraw(void);

/* Switch output to the framebuffer. Must be called before
 * terminal_init() if the framebuffer is available. */
void terminal_enable_fb(void);
int  terminal_fb_active(void);

#endif