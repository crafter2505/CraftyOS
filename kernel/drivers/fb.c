#include "fb.h"
#include "address_space.h"

static volatile uint32_t *fb_base;
static uint32_t fb_pixels_per_scanline;
static uint32_t fb_w;
static uint32_t fb_h;

static const uint32_t vga_palette[16] = {
    0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
    0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
    0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
    0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
};

extern const uint8_t font_8x16[95][16];

/* 16x32 glyphs, generated from font_8x16 at init by 2x upscale. */
static uint8_t font_16x32[95][32][2];

static uint32_t font_scale = 1;

static void build_large_font(void) {
    for (int g = 0; g < 95; g++) {
        for (int row = 0; row < 16; row++) {
            uint8_t bits = font_8x16[g][row];
            uint8_t lo = 0, hi = 0;
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    int out = col * 2;
                    lo |= (uint8_t)(0x80 >> out);
                    hi |= (uint8_t)(0x80 >> (out + 1));
                }
            }
            font_16x32[g][row * 2][0]     = lo;
            font_16x32[g][row * 2][1]     = hi;
            font_16x32[g][row * 2 + 1][0] = lo;
            font_16x32[g][row * 2 + 1][1] = hi;
        }
    }
}

int fb_init(struct boot_info *bi) {
    if (!bi || bi->fb_addr == 0 || bi->fb_width == 0 || bi->fb_height == 0) {
        return 0;
    }

    fb_base = (volatile uint32_t *)(DIRECT_MAP_BASE + bi->fb_addr);
    fb_pixels_per_scanline = bi->fb_pitch / 4;
    fb_w = bi->fb_width;
    fb_h = bi->fb_height;

    build_large_font();
    return 1;
}

uint32_t fb_width(void)  { return fb_w; }
uint32_t fb_height(void) { return fb_h; }

void fb_set_font_scale(uint32_t scale) {
    font_scale = (scale == 2) ? 2 : 1;
}

uint32_t fb_font_scale(void)   { return font_scale; }
uint32_t fb_cell_width(void)   { return 8 * font_scale; }
uint32_t fb_cell_height(void)  { return 16 * font_scale; }

uint32_t vga_palette_lookup(unsigned char idx) {
    return vga_palette[idx & 0x0F];
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_w || y >= fb_h) return;
    fb_base[y * fb_pixels_per_scanline + x] = color;
}

void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_h; y++) {
        for (uint32_t x = 0; x < fb_w; x++) {
            fb_base[y * fb_pixels_per_scanline + x] = color;
        }
    }
}

void fb_clear_palette(unsigned char vga_color) {
    fb_clear(vga_palette[vga_color & 0x0F]);
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t x_end = x + w;
    uint32_t y_end = y + h;
    if (x_end > fb_w) x_end = fb_w;
    if (y_end > fb_h) y_end = fb_h;

    for (uint32_t row = y; row < y_end; row++) {
        for (uint32_t col = x; col < x_end; col++) {
            fb_base[row * fb_pixels_per_scanline + col] = color;
        }
    }
}

/* Copy a rectangular pixel block within the framebuffer. Rows are
 * copied one at a time so overlapping regions (scrolling up) work
 * correctly even if src and dst share memory. */
void fb_copy_rect(uint32_t src_x, uint32_t src_y,
                  uint32_t dst_x, uint32_t dst_y,
                  uint32_t w, uint32_t h) {
    if (src_x + w > fb_w) w = fb_w - src_x;
    if (dst_x + w > fb_w) w = fb_w - dst_x;
    if (src_y + h > fb_h) h = fb_h - src_y;
    if (dst_y + h > fb_h) h = fb_h - dst_y;
    if (w == 0 || h == 0) return;

    if (dst_y <= src_y) {
        for (uint32_t row = 0; row < h; row++) {
            for (uint32_t col = 0; col < w; col++) {
                fb_base[(dst_y + row) * fb_pixels_per_scanline + dst_x + col] =
                    fb_base[(src_y + row) * fb_pixels_per_scanline + src_x + col];
            }
        }
    } else {
        for (uint32_t row = h; row > 0; row--) {
            for (uint32_t col = 0; col < w; col++) {
                fb_base[(dst_y + row - 1) * fb_pixels_per_scanline + dst_x + col] =
                    fb_base[(src_y + row - 1) * fb_pixels_per_scanline + src_x + col];
            }
        }
    }
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 126) uc = '?';

    if (font_scale == 1) {
        const uint8_t *glyph = font_8x16[uc - 32];
        for (int row = 0; row < 16; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
                fb_put_pixel(x + (uint32_t)col, y + (uint32_t)row, color);
            }
        }
    } else {
        const uint8_t *glyph = &font_16x32[uc - 32][0][0];
        for (int row = 0; row < 32; row++) {
            uint16_t bits = ((uint16_t)glyph[row * 2] << 8) | glyph[row * 2 + 1];
            for (int col = 0; col < 16; col++) {
                uint32_t color = (bits & (0x8000 >> col)) ? fg : bg;
                fb_put_pixel(x + (uint32_t)col, y + (uint32_t)row, color);
            }
        }
    }
}

void fb_draw_cell(uint32_t x, uint32_t y, char c, unsigned char attr) {
    uint32_t fg = vga_palette[attr & 0x0F];
    uint32_t bg = vga_palette[(attr >> 4) & 0x0F];
    fb_draw_char(x, y, c, fg, bg);
}