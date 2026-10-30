#include "terminal.h"
#include "vga.h"
#include "fb.h"

#define HISTORY_LINES 1000
#define MAX_COLS      512

static unsigned char history[HISTORY_LINES][MAX_COLS * 2];

static int history_count = 0;
static int history_write = 0;
static int scroll_offset = 0;

static int cursor_row = 0;
static int cursor_col = 0;

static unsigned char current_attr = 0x07;

static int fb_active = 0;

static int term_cols = VGA_WIDTH;
static int term_rows = VGA_HEIGHT;

void terminal_enable_fb(void) { fb_active = 1; }
int  terminal_fb_active(void) { return fb_active; }

static void recompute_grid(void)
{
    if (!fb_active) {
        term_cols = VGA_WIDTH;
        term_rows = VGA_HEIGHT;
        return;
    }
    int cols = (int)(fb_width()  / fb_cell_width());
    int rows = (int)(fb_height() / fb_cell_height());
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    term_cols = cols;
    term_rows = rows;
}

void terminal_set_font_scale(unsigned int scale)
{
    fb_set_font_scale(scale);
    recompute_grid();
    terminal_clear();
}

static void history_write_cell(int line_idx, int col, char c, unsigned char attr)
{
    if (line_idx < 0 || line_idx >= HISTORY_LINES) return;
    if (col < 0 || col >= term_cols) return;
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
    for (int col = 0; col < term_cols; col++) {
        history_write_cell(line_idx, col, ' ', current_attr);
    }
}

/* Draw one visible cell directly. This is the fbcon-equivalent of
 * fbcon_putc: only the changed cell is touched. */
static void draw_visible_cell(int row, int col, char c, unsigned char attr)
{
    if (row < 0 || row >= term_rows) return;
    if (col < 0 || col >= term_cols) return;
    if (fb_active) {
        fb_draw_cell((uint32_t)col * fb_cell_width(),
                     (uint32_t)row * fb_cell_height(), c, attr);
    } else {
        vga_set_cell(row, col, c, attr);
    }
}

/* Full redraw of the visible window. Called on scrollback, clear,
 * and font change — NOT on every putchar. */
static void render_visible(void)
{
    int newest = history_count - 1;
    if (newest < 0) newest = 0;

    int top_line = newest - (term_rows - 1) - scroll_offset;
    if (top_line < 0) top_line = 0;

    for (int row = 0; row < term_rows; row++) {
        int logical = top_line + row;
        for (int col = 0; col < term_cols; col++) {
            char c; unsigned char a;
            if (logical <= newest && logical >= 0) {
                history_get_cell(logical, col, &c, &a);
            } else {
                c = ' ';
                a = current_attr;
            }
            draw_visible_cell(row, col, c, a);
        }
    }

    if (!fb_active) {
        if (scroll_offset == 0) {
            vga_update_cursor(cursor_row, cursor_col);
        } else {
            vga_update_cursor(0, 0);
        }
    }
}

/* Scroll the visible area up by one line. This is the fbcon
 * SCROLL_MOVE equivalent: memmove the pixel block, then clear the
 * new bottom line. No per-cell redraw of the rest of the screen. */
static void scroll_visible_up(void)
{
    if (fb_active) {
        uint32_t cell_w = fb_cell_width();
        uint32_t cell_h = fb_cell_height();
        /* Move everything except the bottom row up by one cell height. */
        fb_copy_rect(0, (uint32_t)cell_h,
                     0, 0,
                     (uint32_t)term_cols * cell_w,
                     (uint32_t)(term_rows - 1) * cell_h);
        /* Clear the new bottom row. */
        fb_fill_rect(0, (uint32_t)(term_rows - 1) * cell_h,
                     (uint32_t)term_cols * cell_w, cell_h,
                     vga_palette_lookup((current_attr >> 4) & 0x0F));
    } else {
        vga_scroll_up(current_attr);
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
    }

    /* If the cursor moved past the bottom, scroll the visible area
     * (fbcon-style blit) instead of redrawing everything. */
    if (scroll_offset == 0 && cursor_row >= term_rows) {
        scroll_visible_up();
        cursor_row = term_rows - 1;
    }
}

void terminal_init(void)
{
    if (fb_active) {
        fb_clear_palette(VGA_BLACK);
    } else {
        vga_init();
    }

    recompute_grid();

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
    if (fb_active) {
        fb_clear_palette((current_attr >> 4) & 0x0F);
    } else {
        vga_clear_screen(current_attr);
        vga_update_cursor(0, 0);
    }
    cursor_row = 0;
    cursor_col = 0;
    scroll_offset = 0;
}

void terminal_putchar(char c)
{
    if (scroll_offset != 0) {
        scroll_offset = 0;
        render_visible();
    }

    int cur_line = (history_write == 0 && history_count == HISTORY_LINES)
                    ? (HISTORY_LINES - 1)
                    : history_write;

    int old_row = cursor_row;
    int old_col = cursor_col;

    if (c == '\n') {
        advance_line();
        /* Only the old cursor position needs clearing if we didn't scroll. */
        if (scroll_offset == 0 && old_row == cursor_row && cursor_row < term_rows) {
            draw_visible_cell(cursor_row, 0, ' ', current_attr);
        }
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            history_write_cell(cur_line, cursor_col, ' ', current_attr);
            draw_visible_cell(cursor_row, cursor_col, ' ', current_attr);
        }
    } else if (c == '\t') {
        int next = (cursor_col + 4) & ~3;
        while (cursor_col < next && cursor_col < term_cols) {
            history_write_cell(cur_line, cursor_col, ' ', current_attr);
            draw_visible_cell(cursor_row, cursor_col, ' ', current_attr);
            cursor_col++;
        }
        if (cursor_col >= term_cols) {
            advance_line();
        }
    } else {
        history_write_cell(cur_line, cursor_col, c, current_attr);
        draw_visible_cell(cursor_row, cursor_col, c, current_attr);
        cursor_col++;
        if (cursor_col >= term_cols) {
            advance_line();
        }
    }

    /* Update hardware cursor in VGA mode; in fb mode, redraw only the
     * cursor cell so it appears at the new position. */
    if (!fb_active) {
        if (scroll_offset == 0) {
            vga_update_cursor(cursor_row, cursor_col);
        }
    } else if (scroll_offset == 0) {
        /* Redraw the new cursor cell (and clear the old one) — this is
         * the only extra cell touch per keystroke. */
        if (old_row != cursor_row || old_col != cursor_col) {
            char oc; unsigned char oa;
            int logical = history_count - 1;
            if (logical >= 0) {
                history_get_cell(logical, old_col, &oc, &oa);
                draw_visible_cell(old_row, old_col, oc, oa);
            }
        }
        char nc; unsigned char na;
        int logical = history_count - 1;
        if (logical >= 0 && cursor_col < term_cols) {
            history_get_cell(logical, cursor_col, &nc, &na);
            draw_visible_cell(cursor_row, cursor_col, nc, na);
        }
    }
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
    int max_offset = history_count - term_rows;
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
    int max_offset = history_count - term_rows;
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