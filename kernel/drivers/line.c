#include "line.h"
#include "terminal.h"
#include "kprintf.h"
#include "sched.h"
#include "thread.h"

static char line_buffer[LINE_MAX];
static int  line_len = 0;
static volatile int line_ready = 0;

/* Thread blocked waiting for input. Set by line_readline before
 * blocking, woken by line_putchar when Enter is pressed. Cleared by
 * line_readline after the line is drained, not by line_putchar, so
 * a second Enter arriving before the reader runs doesn't lose the
 * wakeup. */
static struct thread *waiting_reader = 0;

void line_init(void) {
    line_len = 0;
    line_ready = 0;
    line_buffer[0] = 0;
    waiting_reader = 0;
}

void line_putchar(char c) {
    if (c == '\n' || c == '\r') {
        line_buffer[line_len] = 0;
        line_ready = 1;
        terminal_putchar('\n');

        if (waiting_reader) {
            sched_wake(waiting_reader);
        }
        return;
    }

    if (c == '\b' || c == 127) {
        if (line_len > 0) {
            line_len--;
            terminal_putchar('\b');
            terminal_putchar(' ');
            terminal_putchar('\b');
        }
        return;
    }

    if (c >= 32 && c < 127 && line_len < LINE_MAX - 1) {
        line_buffer[line_len++] = c;
        terminal_putchar(c);
    }
}

int line_readline(char *out, int max) {
    if (!out || max <= 0) return -1;

    while (!line_ready) {
        waiting_reader = thread_current();
        /* Re-check after storing the waiter: if Enter arrived between
         * the loop test and the assignment, the wake would already
         * have been sent and we'd sleep forever. */
        if (line_ready) {
            break;
        }
        sched_block();
    }

    waiting_reader = 0;

    int n = line_len;
    if (n >= max) n = max - 1;
    for (int i = 0; i < n; i++) out[i] = line_buffer[i];
    out[n] = 0;

    line_len = 0;
    line_ready = 0;
    line_buffer[0] = 0;

    return n;
}