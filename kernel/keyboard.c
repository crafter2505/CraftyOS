#include "keyboard.h"
#include "io.h"
#include "kprintf.h"
#include "terminal.h"
#include "types.h"
#include "line.h"

#define KEYBOARD_DATA_PORT 0x60

static const char scancode_to_ascii[] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ',
};

static const char scancode_to_ascii_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ',
};

static int shift_held = 0;

void keyboard_init(void) {
    shift_held = 0;
}

void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_held = 0;
        }
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = 1;
        return;
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char c = shift_held ? scancode_to_ascii_shift[scancode]
                            : scancode_to_ascii[scancode];
        if (c) {
            line_putchar(c);
        }
    }
}