#pragma once

#include "libmmath.h"
#include <stdint.h>


#define PIXELFMT_ARGB8 1

#ifdef PIXELFMT_ARGB8
#define COLOR(a, r, g, b) ((b << 24) | (g << 16) | (r << 8) | a)
#else
#error "No pixel format defined"
#endif

#define COL_BLACK   COLOR(0xff, 0, 0, 0)
#define COL_GRAY    COLOR(0xff, 0x80, 0x80, 0x80)
#define COL_RED     COLOR(0xff, 0xff, 0, 0)
#define COL_GREEN   COLOR(0xff, 0, 0xff, 0)
#define COL_BLUE    COLOR(0xff, 0, 0, 0xff)
#define COL_WHITE   COLOR(0xff, 0xff, 0xff, 0xff)
#define COL_YELLOW  COLOR(0xff, 0xff, 0xff, 0)
#define COL_MAGENTA COLOR(0xff, 0xff, 0, 0xff)
#define COL_CYAN    COLOR(0xff, 0, 0xff, 0xff)


typedef struct Display {
    uint32_t* framebuffer;
    int32_t pitch;
    int32_t width, height;

    void *opaque;
    struct {
        int (*refresh)(struct Display*);
    } ops;
} Display;

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
    uint32_t hmargin, vmargin;
} Font;

void draw_filled_rect(Display* window, int x, int y, int w, int h, uint32_t color);

void draw_outlined_rect(Display* window, int x, int y, int w, int h, int thickness, uint32_t color);

void draw_circle(Display* window, int x, int y, int radius, uint32_t color);

void draw_line(Display* window, int x1, int y1, int x2, int y2, int thickness, uint32_t color);

Font* get_default_font(void);

int load_psf_font(uint8_t const* data, size_t size, Font*);

void draw_char(Display* window, Font* font, char c, int x, int y, int scale, uint32_t color);

void draw_text(Display* window, Font* font, char const* text, int x, int y, int scale, uint32_t color);
