#include "line.h"
#include "terminal.h"
#include "kprintf.h"

static char buffer[LINE_MAX];
static int length = 0;
static int submitted = 0;

void line_init(void) {
    length = 0;
    submitted = 0;
    buffer[0] = 0;
}

void line_putchar(char c) {
    if (c == '\n') {
        buffer[length] = 0;
        submitted = 1;
        terminal_putchar('\n');
        kprintf("Received: %s\n", buffer);
        kprintf("craftyOS> ");
        length = 0;
        return;
    }
    if (c == '\b') {
        if (length > 0) {
            length--;
            terminal_putchar('\b');
            terminal_putchar(' ');
            terminal_putchar('\b');
        }
        return;
    }

    if (length < LINE_MAX - 1) {
        buffer[length++] = c;
        terminal_putchar(c);
    }
}

const char *line_get(void) {
    if (submitted) {
        submitted = 0;
        return buffer;
    }
    return 0;
}