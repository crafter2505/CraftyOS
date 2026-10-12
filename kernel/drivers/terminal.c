#include "terminal.h"
#include "vga.h"

#define HISTORY_LINES 1000

static unsigned char history[HISTORY_LINES][VGA_WIDTH * 2];

static int history_count = 0;

static int history_write = 0;

static int scroll_offset = 0;

static int cursor_row = 0;
static int cursor_col = 0;

static unsigned char current_attr = 0x07;


static void history_write_cell(int line_idx, int col, char c, unsigned char attr)
{
    if (line_idx < 0 || line_idx >= HISTORY_LINES) return;
    if (col < 0 || col >= VGA_WIDTH) return;
    history[line_idx][col * 2]     = (unsigned char)c;
    history[line_idx][col * 2 + 1] = attr;
}

static void history_get_cell(int logical_line, int col, char *out_c, unsigned char *out_attr)
{
    int start = (history_count < HISTORY_LINES) ? 0 : history_write;
    int idx = (start + logical_line) % HISTORY_LINES;
    *out_c    = history[idx][col * 2];
    *out_attr = history[idx][col * 2 + 1];
}

static void history_clear_line(int line_idx)
{
    for (int col = 0; col < VGA_WIDTH; col++) {
        history_write_cell(line_idx, col, ' ', current_attr);
    }
}


static void render_visible(void)
{
    int newest = history_count - 1;
    if (newest < 0) newest = 0;

    int top_line = newest - (VGA_HEIGHT - 1) - scroll_offset;
    if (top_line < 0) top_line = 0;

    for (int row = 0; row < VGA_HEIGHT; row++) {
        int logical = top_line + row;
        for (int col = 0; col < VGA_WIDTH; col++) {
            char c; unsigned char a;
            if (logical <= newest && logical >= 0) {
                history_get_cell(logical, col, &c, &a);
            } else {
                c = ' ';
                a = current_attr;
            }
            vga_set_cell(row, col, c, a);
        }
    }

    if (scroll_offset == 0) {
        vga_update_cursor(cursor_row, cursor_col);
    } else {
        vga_update_cursor(0, 0);
    }
}


static void advance_line(void)
{
    cursor_col = 0;

    if (scroll_offset == 0) {
        cursor_row++;
    }

    if (history_count < HISTORY_LINES) {
        history_count++;
        history_write = (history_write + 1) % HISTORY_LINES;
        int idx_to_clear = (history_write == 0 && history_count == HISTORY_LINES)
                            ? 0
                            : history_write;
        history_clear_line(idx_to_clear);
    } else {
        history_write = (history_write + 1) % HISTORY_LINES;
        history_clear_line(history_write);
        if (scroll_offset == 0 && cursor_row >= VGA_HEIGHT) {
            cursor_row = VGA_HEIGHT - 1;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        cursor_row = VGA_HEIGHT - 1;
    }
}


void terminal_init(void)
{
    vga_init();
    history_count = 0;
    history_write = 0;
    scroll_offset = 0;
    cursor_row = 0;
    cursor_col = 0;
    current_attr = vga_make_attr(VGA_LIGHT_GREY, VGA_BLACK);

    history_count = 1;
    history_write = 0;
    history_clear_line(0);
}

void terminal_clear(void)
{
    vga_clear_screen(current_attr);
    cursor_row = 0;
    cursor_col = 0;
    scroll_offset = 0;
    vga_update_cursor(cursor_row, cursor_col);
}

void terminal_putchar(char c)
{
    if (scroll_offset != 0) {
        scroll_offset = 0;
    }

    int cur_line = (history_write == 0 && history_count == HISTORY_LINES)
                    ? (HISTORY_LINES - 1)
                    : history_write;

    if (c == '\n') {
        advance_line();
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            history_write_cell(cur_line, cursor_col, ' ', current_attr);
        }
    } else if (c == '\t') {
        int next = (cursor_col + 4) & ~3;
        while (cursor_col < next && cursor_col < VGA_WIDTH) {
            history_write_cell(cur_line, cursor_col, ' ', current_attr);
            cursor_col++;
        }
        if (cursor_col >= VGA_WIDTH) {
            advance_line();
        }
    } else {
        history_write_cell(cur_line, cursor_col, c, current_attr);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            advance_line();
        }
    }

    render_visible();
}

void terminal_puts(const char *s)
{
    while (*s) terminal_putchar(*s++);
}

void terminal_set_color(unsigned char fg, unsigned char bg)
{
    current_attr = vga_make_attr(fg, bg);
}


void terminal_scrollback_up(int lines)
{
    int max_offset = history_count - VGA_HEIGHT;
    if (max_offset < 0) max_offset = 0;
    scroll_offset += lines;
    if (scroll_offset > max_offset) scroll_offset = max_offset;
    render_visible();
}

void terminal_scrollback_down(int lines)
{
    scroll_offset -= lines;
    if (scroll_offset < 0) scroll_offset = 0;
    render_visible();
}

void terminal_scrollback_top(void)
{
    int max_offset = history_count - VGA_HEIGHT;
    if (max_offset < 0) max_offset = 0;
    scroll_offset = max_offset;
    render_visible();
}

void terminal_scrollback_bottom(void)
{
    scroll_offset = 0;
    render_visible();
}

int terminal_is_scrolled_back(void)
{
    return scroll_offset != 0;
}

void terminal_redraw(void)
{
    render_visible();
}