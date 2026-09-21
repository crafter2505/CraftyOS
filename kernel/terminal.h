#ifndef TERMINAL_H
#define TERMINAL_H
#include "vga.h"
void terminal_init(void);
void terminal_clear(void);
void terminal_putchar(char c);
void terminal_puts(const char *s);
void terminal_set_color(unsigned char fg, unsigned char bg);

#endif