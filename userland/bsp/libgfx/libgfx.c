#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <api/syscalls.h>
#include "libgfx.h"


#define PSF_MAGIC 0x864ab572


extern const uint8_t _resource_default_psf_font[];
extern const size_t _resource_default_psf_font_size;
static Font g_default_font;

static void _draw_circle(Display *display, int x, int y, int radius, uint32_t color);
static void _draw_line(Display *display, int x1, int y1, int x2, int y2, int thickness, uint32_t color);

inline static void set_pixel(Display* display, int x, int y, uint32_t color)
{
    if (x >= 0 && x < display->width && y >= 0 && y < display->height) {
        display->framebuffer[y * display->width + x] = color;
    }
}

void draw_filled_rect(Display *display, int x, int y, int w, int h, uint32_t color)
{
    int x1 = MAX(0, MIN(x, x + w));
    int x2 = MIN(display->width, MAX(x, x + w));
    int y1 = MAX(0, MIN(y, y + h));
    int y2 = MIN(display->height, MAX(y, y + h));

    for (int y = y1; y < y2; y++) {
        uint32_t *fb = &display->framebuffer[y * display->width + x1];
        for (int x = x1; x < x2; x++, fb++) {
            *fb = color;
        }
    }
}


void draw_outlined_rect(Display *display, int x, int y, int w, int h, int thickness, uint32_t color)
{
    int x1 = MAX(0, MIN(x, x + w));
    int x2 = MIN(display->width, MAX(x, x + w));
    int y1 = MAX(0, MIN(y, y + h));
    int y2 = MIN(display->height, MAX(y, y + h));

    printf("x1: %d, x2: %d, y1: %d, y2: %d\n", x1, x2, y1, y2);

    for (int i = 0; i < thickness; i++) {
        for (int _x = x1; _x < x2; _x++) {
            set_pixel(display, _x, y1 + i, color);
            set_pixel(display, _x, y2 - i, color);
        }
    }

    for (int i = 0; i < thickness; i++) {
        for (int _y = y1; _y < y2; _y++) {
            set_pixel(display, x1 + i, _y, color);
            set_pixel(display, x2 - i, _y, color);
        }
    }
}

static void _draw_circle(Display *display, int x, int y, int radius, uint32_t color)
{
    int cx = radius;
    int cy = 0;
    int radius_error = 1 - cx;

    while (cx >= cy) {
        set_pixel(display, x + cx, y + cy, color);
        set_pixel(display, x - cx, y + cy, color);
        set_pixel(display, x + cx, y - cy, color);
        set_pixel(display, x - cx, y - cy, color);
        set_pixel(display, x + cy, y + cx, color);
        set_pixel(display, x - cy, y + cx, color);
        set_pixel(display, x + cy, y - cx, color);
        set_pixel(display, x - cy, y - cx, color);

        cy++;

        if (radius_error < 0) {
            radius_error += 2 * cy + 1;
        } else {
            cx--;
            radius_error += 2 * (cy - cx + 1);
        }
    }
}

void draw_circle(Display *display, int x, int y, int radius, uint32_t color)
{
    _draw_circle(display, x, y, radius, color);
}

static void _draw_line(Display *display, int x1, int y1, int x2, int y2, int thickness, uint32_t color)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx, sy;
    if (x1 < x2) {
        sx = 1;
    } else {
        sx = -1;
    }
    if (y1 < y2) {
        sy = 1;
    } else {
        sy = -1;
    }

    int err = dx - dy;
    int e2;
    int x = x1, y = y1;

    int half_thickness = thickness / 2;

    while (1) {
        // Draw a filled rectangle to simulate line thickness
        for (int i = -half_thickness; i <= half_thickness; i++) {
            for (int j = -half_thickness; j <= half_thickness; j++) {
                set_pixel(display, x + i, y + j, color);
            }
        }

        if (x == x2 && y == y2) {
            break;
        }

        e2 = 2 * err;
        if (e2 > -dy) {
            err = err - dy;
            x = x + sx;
        }

        if (e2 < dx) {
            err = err + dx;
            y = y + sy;
        }
    }
}

void draw_line(Display *display, int x1, int y1, int x2, int y2, int thickness, uint32_t color)
{
    _draw_line(display, x1, y1, x2, y2, thickness, color);
}

Font *get_default_font(void)
{
    if (g_default_font.header.magic != PSF_MAGIC) {
        load_psf_font(_resource_default_psf_font, _resource_default_psf_font_size, &g_default_font);
    }

    return &g_default_font;
}

int load_psf_font(uint8_t const* data, size_t size, Font *font)
{
    if (size < sizeof(font->header))
        return -1;

    memcpy(&font->header, data, sizeof(font->header));
    if (font->header.magic != PSF_MAGIC)
        return -1;

    font->data = data;
    font->size = size;
    font->hmargin = 2;
    font->vmargin = 4;

    return 0;
}

void draw_char(Display *display, Font *font, char c, int x, int y, int scale, uint32_t color)
{
    if (c >= font->header.num_glyphs) return;
    if (c < ' ' || c > '~') return;

    uint8_t const* start_of_glyph = &font->data[sizeof(font->header) + font->header.bytes_per_glyph * c];

    for (size_t glyph_y = 0; glyph_y < font->header.height; glyph_y++) {
        uint8_t glyph_row = start_of_glyph[glyph_y];
        for (size_t glyph_x = 0; glyph_x < 8; glyph_x++) {
            if (glyph_row & (1 << (7 - glyph_x))) {
                if (scale == 1)
                    set_pixel(display, x + scale * glyph_x, y + scale * glyph_y, color);
                else
                    draw_filled_rect(display, x + scale * glyph_x, y + scale * glyph_y, scale, scale, color);
            }
        }
    }
}

void draw_text(Display *display, Font *font, const char *text, int x, int y, int scale, uint32_t color)
{
    while (*text && x < display->width) {
        draw_char(display, font, *text, x, y, scale, color);
        text++;
        x += (font->header.width + font->hmargin) * scale;
    }
}

uint32_t get_opposite_color(uint32_t color)
{
    uint32_t a = GET_ALPHA(color);
    uint32_t r = GET_RED(color);
    uint32_t g = GET_GREEN(color);
    uint32_t b = GET_BLUE(color);
    
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
    
    return COLOR(a, r, g, b);
}
