#include <stdlib.h>
#include <api/syscalls.h>
#include "libgfx.h"


Window open_window(const char *title, int width, int height)
{
    uint32_t *framebuffer = malloc(width * height * sizeof(uint32_t));

    return (Window) {
        .window_title = title,
        .framebuffer = framebuffer,
        .x = 0,
        .y = 0,
        .width = width,
        .height = height
    };
}

void refresh_window(Window *window)
{
    syscall(
        SYS_BlitFramebuffer, 
        (uint32_t) window->framebuffer,
        (uint32_t) window->x,
        (uint32_t) window->y,
        (uint32_t) window->width,
        (uint32_t) window->height
    );
}

void draw_filled_rect(Window *window, int x, int y, int w, int h, uint32_t color)
{
    int x1 = MAX(0, MIN(x, x + w));
    int x2 = MIN(window->width, MAX(x, x + w));
    int y1 = MAX(0, MIN(y, y + h));
    int y2 = MAX(window->height, MAX(y, y + w));

    for (int _y = y1; _y <= y2; _y++) {
        uint32_t *fb = &window->framebuffer[_y * window->width + x1];
        for (int _x = x1; _x <= x2; _x++, fb++) {
            *fb = color;
        }
    }
}

static void set_pixel(Window* window, int x, int y, uint32_t color)
{
    if (x >= 0 && x < window->width && y >= 0 && y < window->height) {
        window->framebuffer[y * window->width + x] = color;
    }
}

void draw_circle(Window *window, int x, int y, int radius, uint32_t color)
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

void draw_line(Window *window, int x1, int y1, int x2, int y2, int thickness, uint32_t color)
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
