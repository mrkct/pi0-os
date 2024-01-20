#pragma once

#include "libmmath.h"
#include <stdint.h>

enum {
    COL_BLACK = 0xff000000,
    COL_RED = 0xff0000ff,
    COL_GREEN = 0xff00ff00,
    COL_BLUE = 0xffff0000,
    COL_WHITE = 0xffffffff
};

typedef struct Window {
    char const* window_title;
    uint32_t* framebuffer;
    int x, y;
    int width, height;
    int y_offset;
} Window;

typedef struct PSFFont {
    struct {
        uint32_t magic;
        uint32_t version;
        uint32_t header_size;
        uint32_t has_unicode_table;
        uint32_t num_glyphs;
        uint32_t bytes_per_glyph;
        uint32_t height;
        uint32_t width;
    } header;

    uint8_t const* data;
    size_t size;
} Font;

Window open_window(char const* title, int width, int height, bool show_titlebar);

void refresh_window(Window*);

void draw_filled_rect(Window* window, int x, int y, int w, int h, uint32_t color);

void draw_outlined_rect(Window* window, int x, int y, int w, int h, int thickness, uint32_t color);

void draw_circle(Window* window, int x, int y, int radius, uint32_t color);

void draw_line(Window* window, int x1, int y1, int x2, int y2, int thickness, uint32_t color);

Font* get_default_font(void);

int load_psf_font(uint8_t const* data, size_t size, Font*);

void draw_char(Window* window, Font* font, char c, int x, int y, uint32_t color);

void draw_text(Window* window, Font* font, char const* text, int x, int y, uint32_t color);
