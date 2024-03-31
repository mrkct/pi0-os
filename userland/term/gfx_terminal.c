#include <stdio.h>
#include <string.h>
#include <api/syscalls.h>
#include "gfx_terminal.h"


const int LEFT_PADDING = 4;
const int TOP_PADDING = 6;
const int GFX_CHAR_WIDTH = 8;
const int GFX_CHAR_HEIGHT = 12;
const int TAB_SIZE = 4;

struct {
    char *data;
    int w, h;
    struct {
        int x, y;
    } cursor;

    bool framebuffer_updated;
    Window window;
} g_terminal;

static void gfx_put_char(Window *w, int x, int y, char c, uint32_t background_color, uint32_t foreground_color)
{
    draw_filled_rect(
        w,
        LEFT_PADDING + x * GFX_CHAR_WIDTH,
        TOP_PADDING + y * GFX_CHAR_HEIGHT,
        GFX_CHAR_WIDTH,
        GFX_CHAR_HEIGHT,
        background_color);

    char text[2] = { c, '\0' };

    draw_text(
        w,
        get_default_font(),
        text,
        LEFT_PADDING + x * GFX_CHAR_WIDTH,
        TOP_PADDING + y * GFX_CHAR_HEIGHT,
        foreground_color);
}

static void redraw_all(void)
{
    for (int y = 0; y < g_terminal.h; y++) {
        for (int x = 0; x < g_terminal.w; x++) {
            gfx_put_char(
                &g_terminal.window,
                x, y,
                g_terminal.data[y * g_terminal.w + x],
                COL_BACKGROUND,
                COL_FOREGROUND);
        }
    }
}

static void _term_putchar(char c, uint32_t background_color, uint32_t foreground_color)
{
    switch (c) {
    case '\r':
        g_terminal.cursor.x = 0;
        break;
    case '\n':
        g_terminal.cursor.x = 0;
        g_terminal.cursor.y++;
        break;
    case '\b':
    case 127:
        if (g_terminal.cursor.x == 0)
            return;
        g_terminal.cursor.x--;
        g_terminal.data[g_terminal.cursor.y * g_terminal.w + g_terminal.cursor.x] = ' ';
        gfx_put_char(
            &g_terminal.window,
            g_terminal.cursor.x,
            g_terminal.cursor.y,
            ' ',
            background_color,
            foreground_color);
        break;
    case '\t': {
        int spaces = g_terminal.cursor.x % TAB_SIZE == 0 ? TAB_SIZE : TAB_SIZE - (g_terminal.cursor.x % 4);
        for (int i = 0; i < spaces; i++)
            _term_putchar(' ', foreground_color, background_color);
        return;
    }
    default:
        g_terminal.data[g_terminal.cursor.y * g_terminal.w + g_terminal.cursor.x] = c;
        gfx_put_char(
            &g_terminal.window,
            g_terminal.cursor.x,
            g_terminal.cursor.y,
            c,
            background_color,
            foreground_color);
        g_terminal.cursor.x++;
        break;
    }
    
    if (g_terminal.cursor.x == g_terminal.w) {
        g_terminal.cursor.x = 0;
        g_terminal.cursor.y++;
    }

    if (g_terminal.cursor.y == g_terminal.h) {
        for (int i = 1; i < g_terminal.h; i++) {
            memcpy(&g_terminal.data[(i-1) * g_terminal.w], &g_terminal.data[i * g_terminal.w], g_terminal.w);
        }
        memset(&g_terminal.data[(g_terminal.h - 1)], ' ', g_terminal.w);
        g_terminal.cursor.y = g_terminal.h - 1;
        redraw_all();
    }
}

void gfx_terminal_print(const char *s, uint32_t background_color, uint32_t foreground_color)
{
    while (*s) {
        _term_putchar(*s, background_color, foreground_color);
        s++;
    }
    g_terminal.framebuffer_updated = true;
}

void gfx_update_window(void)
{
    if (g_terminal.framebuffer_updated)
        refresh_window(&g_terminal.window);
    g_terminal.framebuffer_updated = false;
}

void gfx_terminal_init(void)
{
    g_terminal.window = open_window("Terminal", 640, 480, false);
    g_terminal.cursor.x = 0;
    g_terminal.cursor.y = 0;
    g_terminal.w = (g_terminal.window.width - 2*LEFT_PADDING) / GFX_CHAR_WIDTH;
    g_terminal.h = (g_terminal.window.height - 2*TOP_PADDING) / GFX_CHAR_HEIGHT;
    g_terminal.data = malloc(g_terminal.w * g_terminal.h);
    memset(g_terminal.data, ' ', g_terminal.w * g_terminal.h);
    g_terminal.framebuffer_updated = true;

    draw_filled_rect(&g_terminal.window, 0, 0, g_terminal.window.width, g_terminal.window.height, COL_BACKGROUND);
    redraw_all();
    gfx_update_window();
}
