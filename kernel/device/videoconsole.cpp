#include <kernel/device/videoconsole.h>
#include <kernel/lib/memory.h>
#include <kernel/lib/psf/psf.h>

namespace kernel {

static constexpr uint32_t WHITE = 0xffffffff;

static void draw_background(VideoConsole&);
static bool is_character_printable(char c);
static void redraw_all(VideoConsole&);
static void draw_character(VideoConsole&, char c, uint32_t color, size_t x, size_t y);
static void shift_all_lines_up(VideoConsole&);

static bool is_character_printable(char c)
{
    return c >= 32 && c < 127;
}

static void draw_background(VideoConsole& console)
{
    for (size_t i = 0; i < console.fb.height; ++i)
        for (size_t j = 0; j < console.fb.width; ++j)
            console.fb.address[i * console.fb.width + j] = i * j;
}

static void redraw_all(VideoConsole& console)
{
    draw_background(console);

    for (size_t y = 0; y < CONSOLE_HEIGHT; y++)
        for (size_t x = 0; x < CONSOLE_WIDTH; x++)
            draw_character(console, console.data[y * CONSOLE_WIDTH + x], WHITE, x, y);
}

static void draw_character(VideoConsole& console, char c, uint32_t color, size_t x, size_t y)
{
    if (c == ' ')
        return;
    klib::psf_draw_char(console.font, console.fb, c, color, console.offset_x + x * 8, console.offset_y + y * 16);
}

static void shift_all_lines_up(VideoConsole& console)
{
    for (size_t y = 0; y < CONSOLE_HEIGHT - 1; y++)
        for (size_t x = 0; x < CONSOLE_WIDTH; x++)
            console.data[y * CONSOLE_WIDTH + x] = console.data[(y + 1) * CONSOLE_WIDTH + x];

    for (size_t x = 0; x < CONSOLE_WIDTH; x++)
        console.data[(CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH + x] = ' ';
}

Error videoconsole_init(VideoConsole& console, Framebuffer fb, size_t offset_x, size_t offset_y)
{
    console.fb = fb;
    console.cursor_x = 0;
    console.cursor_y = 0;
    console.offset_x = offset_x;
    console.offset_y = offset_y;

    klib::kmemset(console.data, 0, sizeof(console.data));
    TRY(klib::psf_load_default(console.font));
    draw_background(console);

    return Success;
}

void videoconsole_putc(VideoConsole& console, char c)
{
    size_t char_x = console.cursor_x;
    size_t char_y = console.cursor_y;
    bool requires_full_redraw = false;

    switch (c) {
    case '\n': {
        console.data[console.cursor_y * CONSOLE_WIDTH + console.cursor_x] = ' ';
        console.cursor_x = 0;
        console.cursor_y++;
        break;
    }
    case '\t': {
        size_t spaces_to_print = TAB_SIZE - (console.cursor_x % TAB_SIZE);
        for (size_t i = 0; i < spaces_to_print; i++)
            videoconsole_putc(console, ' ');
        return;
    }
    default:
        if (!is_character_printable(c))
            c = '?';

        console.data[console.cursor_y * CONSOLE_WIDTH + console.cursor_x] = c;
        console.cursor_x++;
    }

    if (console.cursor_x >= CONSOLE_WIDTH) {
        console.cursor_x = 0;
        console.cursor_y++;
    }
    if (console.cursor_y >= CONSOLE_HEIGHT) {
        shift_all_lines_up(console);
        requires_full_redraw = true;
        console.cursor_y = CONSOLE_HEIGHT - 1;
    }

    if (requires_full_redraw)
        redraw_all(console);
    else
        draw_character(console, console.data[char_y * CONSOLE_WIDTH + char_x], WHITE, char_x, char_y);
}

void videoconsole_clear(VideoConsole& console)
{
    klib::kmemset(console.data, 0, sizeof(console.data));
    redraw_all(console);
}

}
