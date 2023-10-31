#pragma once 

#include <stdint.h>
#include "libmmath.h"


enum {
    COL_BLACK = 0x00000000,
    COL_RED   = 0xff000000,
    COL_GREEN = 0x00ff0000,
    COL_BLUE  = 0x0000ff00,
    COL_WHITE = 0xffffffff
};


typedef struct Window {
    const char *window_title;
    uint32_t *framebuffer;
    int x, y;
    int width, height;
} Window;

Window open_window(const char *title, int width, int height);

void refresh_window(Window*);

void draw_filled_rect(Window *window, int x, int y, int w, int h, uint32_t color);

void draw_circle(Window *window, int x, int y, int radius, uint32_t color);

void draw_line(Window *window, int x1, int y1, int x2, int y2, int thickness, uint32_t color);
