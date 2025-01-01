#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <api/syscalls.h>
#include "libgfx.h"


#define PSF_MAGIC 0x864ab572


#define TITLEBAR_HEIGHT             24
#define TITLEBAR_BORDER_COLOR       0xff333333
#define TITLEBAR_BACKGROUND_COLOR   0xff888888
#define TITLEBAR_TEXT_COLOR         COL_WHITE

extern const uint8_t _resource_default_psf_font[];
extern const size_t _resource_default_psf_font_size;
static Font g_default_font;

static void draw_titlebar(Window *window);
static void _draw_filled_rect(Window *window, int x, int y, int w, int h, uint32_t color);
static void _draw_outlined_rect(Window *window, int x, int y, int w, int h, int thickness, uint32_t color);
static void _draw_circle(Window *window, int x, int y, int radius, uint32_t color);
static void _draw_line(Window *window, int x1, int y1, int x2, int y2, int thickness, uint32_t color);
static void _draw_char(Window *window, Font *font, char c, int x, int y, uint32_t color);
static void _draw_text(Window *window, Font *font, const char *text, int x, int y, uint32_t color);

inline static void set_pixel(Window* window, int x, int y, uint32_t color)
{
    if (x >= 0 && x < window->width && y >= 0 && y < window->height) {
        window->framebuffer[y * window->width + x] = color;
    }
}

Window open_window(const char *title, int width, int height, bool show_titlebar)
{
    int alloc_height = height;
    if (show_titlebar)
        alloc_height += TITLEBAR_HEIGHT;

    uint32_t *framebuffer = malloc(width * alloc_height * sizeof(uint32_t));

    Window window = (Window) {
        .window_title = title,
        .framebuffer = framebuffer,
        .x = 0,
        .y = 0,
        .y_offset = show_titlebar ? TITLEBAR_HEIGHT : 0,
        .width = width,
        .height = height
    };
    if (show_titlebar)
        draw_titlebar(&window);
    
    // TODO: Implement opening the window
    int rc = -1;
    if (rc != 0) {
        fprintf(stderr, "Failed to create window. Rc: %d\r\n", rc);
        exit(-1);
    }
    
    return window;
}

int refresh_window(Window*)
{
    // TODO: Implement this
    return -1; 
}

static void _draw_filled_rect(Window *window, int x, int y, int w, int h, uint32_t color)
{
    int x1 = MAX(0, MIN(x, x + w));
    int x2 = MIN(window->width, MAX(x, x + w));
    int y1 = MAX(0, MIN(y, y + h));
    int y2 = MIN(window->height, MAX(y, y + w));

    for (int _y = y1; _y < y2; _y++) {
        uint32_t *fb = &window->framebuffer[_y * window->width + x1];
        for (int _x = x1; _x < x2; _x++, fb++) {
            *fb = color;
        }
    }
}

void draw_filled_rect(Window *window, int x, int y, int w, int h, uint32_t color)
{
    _draw_filled_rect(window, x, y + window->y_offset, w, h, color);
}

static void _draw_outlined_rect(Window *window, int x, int y, int w, int h, int thickness, uint32_t color)
{
    int x1 = MAX(0, MIN(x, x + w));
    int x2 = MIN(window->width, MAX(x, x + w));
    int y1 = MAX(0, MIN(y, y + h));
    int y2 = MAX(window->height, MAX(y, y + w));

    for (int i = 0; i < thickness; i++) {
        for (int _x = x1; _x < x2; _x++) {
            set_pixel(window, _x, y1 + i, color);
            set_pixel(window, _x, y2 - i, color);
        }
    }

    for (int i = 0; i < thickness; i++) {
        for (int _y = y1; _y < y2; _y++) {
            set_pixel(window, x1 + i, _y, color);
            set_pixel(window, x2 - i, _y, color);
        }
    }
}

void draw_outlined_rect(Window *window, int x, int y, int w, int h, int thickness, uint32_t color)
{
    _draw_outlined_rect(window, x, y + window->y_offset, w, h, thickness, color);
}

static void _draw_circle(Window *window, int x, int y, int radius, uint32_t color)
{
    int cx = radius;
    int cy = 0;
    int radius_error = 1 - cx;

    while (cx >= cy) {
        set_pixel(window, x + cx, y + cy, color);
        set_pixel(window, x - cx, y + cy, color);
        set_pixel(window, x + cx, y - cy, color);
        set_pixel(window, x - cx, y - cy, color);
        set_pixel(window, x + cy, y + cx, color);
        set_pixel(window, x - cy, y + cx, color);
        set_pixel(window, x + cy, y - cx, color);
        set_pixel(window, x - cy, y - cx, color);

        cy++;

        if (radius_error < 0) {
            radius_error += 2 * cy + 1;
        } else {
            cx--;
            radius_error += 2 * (cy - cx + 1);
        }
    }
}

void draw_circle(Window *window, int x, int y, int radius, uint32_t color)
{
    _draw_circle(window, x, y + window->y_offset, radius, color);
}

static void _draw_line(Window *window, int x1, int y1, int x2, int y2, int thickness, uint32_t color)
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
                set_pixel(window, x + i, y + j, color);
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

void draw_line(Window *window, int x1, int y1, int x2, int y2, int thickness, uint32_t color)
{
    _draw_line(window, x1, y1 + window->y_offset, x2, y2 + window->y_offset, thickness, color);
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

    return 0;
}

static void _draw_char(Window *window, Font *font, char c, int x, int y, uint32_t color)
{
    if (c >= font->header.num_glyphs) return;
    if (c < ' ' || c > '~') return;

    uint8_t const* start_of_glyph = &font->data[sizeof(font->header) + font->header.bytes_per_glyph * c];

    for (size_t glyph_y = 0; glyph_y < font->header.height; glyph_y++) {
        uint8_t glyph_row = start_of_glyph[glyph_y];
        for (size_t glyph_x = 0; glyph_x < 8; glyph_x++) {
            if (glyph_row & (1 << (7 - glyph_x)))
                set_pixel(window, x + glyph_x, y + glyph_y, color);
        }
    }
}

void draw_char(Window *window, Font *font, char c, int x, int y, uint32_t color)
{
    _draw_char(window, font, c, x, y + window->y_offset, color);
}

static void _draw_text(Window *window, Font *font, const char *text, int x, int y, uint32_t color)
{
    while (*text && x < window->width) {
        _draw_char(window, font, *text, x, y, color);
        text++;
        x += 8;
    }
}

void draw_text(Window *window, Font *font, const char *text, int x, int y, uint32_t color)
{
    _draw_text(window, font, text, x, y + window->y_offset, color);
}

static void draw_titlebar(Window *window)
{
    _draw_outlined_rect(window, 0, 0, window->width, TITLEBAR_HEIGHT, 2, TITLEBAR_BORDER_COLOR);
    _draw_filled_rect(window, 3, 3, window->width - 3*2, TITLEBAR_HEIGHT - 3*2, TITLEBAR_BACKGROUND_COLOR);
    _draw_text(window, get_default_font(), window->window_title, 12, 10, COL_WHITE);
}
