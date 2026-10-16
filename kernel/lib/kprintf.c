#include "kprintf.h"
#include "vga.h"
#include "terminal.h"
#include <stdarg.h>
#include "serial.h"

static void kputchar(char c) {
    terminal_putchar(c);
    serial_putchar(c);
}

static void kputs(const char *s) {
    while (*s) {
        kputchar(*s++);
    }
}
static void print_uint(unsigned long value, int base, int uppercase) {
    char buf[32];
    int i = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0) {
        kputchar('0');
        return;
    }
    while (value > 0) {
        buf[i++] = digits[value % base];
        value /= base;
    }
    while (i > 0) {
        kputchar(buf[--i]);
    }
}
static void print_int(long value) {
    if (value < 0) {
        kputchar('-');
        print_uint((unsigned long)(-value), 10, 0);
    } else {
        print_uint((unsigned long)value, 10, 0);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            kputchar(*fmt++);
            continue;
        }
         fmt++;

        switch (*fmt) {
        case 'c': {
            char c = (char)va_arg(args, int);
            kputchar(c);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            kputs(s);
            break;
        }
        case 'd': {
            long d = va_arg(args, long);
            print_int(d);
            break;
        }
        case 'u': {
            unsigned long u = va_arg(args, unsigned long);
            print_uint(u, 10, 0);
            break;
        }
        case 'x': {
            unsigned long x = va_arg(args, unsigned long);
            print_uint(x, 16, 0);
            break;
        }
        case 'X': {
            unsigned long x = va_arg(args, unsigned long);
            print_uint(x, 16, 1);
            break;
        }
        case '%': {
            kputchar('%');
            break;
        }
        case '\0': {
            va_end(args);
            return;
        }
        default: {
             kputchar('%');
            kputchar(*fmt);
            break;
        }
        }
        fmt++;
    }
    va_end(args);
}