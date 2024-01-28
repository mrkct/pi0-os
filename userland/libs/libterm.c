#include <stdio.h>
#include <string.h>
#include <api/syscalls.h>
#include "libterm.h"
#include "libgfx.h"


const int GFX_CHAR_WIDTH = 8;
const int GFX_CHAR_HEIGHT = 12;
const int TAB_SIZE = 4;
const uint32_t COL_BACKGROUND = COL_BLUE;
const uint32_t COL_FOREGROUND = COL_WHITE;

struct {
    char *data;
    int w, h;
    struct {
        int x, y;
    } cursor;
} g_terminal;

struct TerminalBackend {
    int (*get_width)(void*);
    int (*get_height)(void*);
    void (*put_char)(void*, int x, int y, char c, uint32_t background_color, uint32_t foreground_color);
    void (*refresh)(void*);
    void *impl_data;
};

static void gfx_backend_init(struct TerminalBackend **);
static int gfx_get_width(void *data);
static int gfx_get_height(void *data);
static void gfx_put_char(void *data, int x, int y, char c, uint32_t background_color, uint32_t foreground_color);
static void gfx_refresh(void *data);
static void term_refresh(void);
static void term_redraw_all(void);
static void _term_putchar(char c);
static int term_stdout_stderr_override(char const *s, int len);
static KeyEvent wait_for_key_event(void);

extern int(*s_stdout_print_func)(char const *, int);
extern int(*s_stderr_print_func)(char const *, int);


static struct TerminalBackend g_graphical_backend = {
    .get_width = gfx_get_width,
    .get_height = gfx_get_height,
    .put_char = gfx_put_char,
    .refresh = gfx_refresh,
    .impl_data = NULL
};

static struct TerminalBackend *g_backend = NULL;

static void gfx_backend_init(struct TerminalBackend **backend)
{
    Window *window = malloc(sizeof(Window));
    Window w = open_window("Terminal", 640, 480, false);
    memcpy(window, &w, sizeof(w));
    g_graphical_backend.impl_data = window;
    draw_filled_rect(window, 0, 0, 640, 480, COL_BACKGROUND);

    *backend = &g_graphical_backend;
}

static int gfx_get_width(void *data)
{
    return ((Window*) data)->width / GFX_CHAR_WIDTH;
}

static int gfx_get_height(void *data)
{
    return ((Window*) data)->height / GFX_CHAR_HEIGHT;
}

static void gfx_put_char(void *data, int x, int y, char c, uint32_t background_color, uint32_t foreground_color)
{
    Window *w = data;
    draw_filled_rect(w, x * GFX_CHAR_WIDTH, y * GFX_CHAR_HEIGHT, GFX_CHAR_WIDTH, GFX_CHAR_HEIGHT, background_color);
    char text[2] = { c, '\0' };
    draw_text(w, get_default_font(), text, x * GFX_CHAR_WIDTH, y * GFX_CHAR_HEIGHT, foreground_color);
}

static void gfx_refresh(void *data)
{
    refresh_window(data);
}

static void term_refresh(void)
{
    g_backend->refresh(g_backend->impl_data);
}

static void term_redraw_all(void)
{
    for (int y = 0; y < g_terminal.h; y++) {
        for (int x = 0; x < g_terminal.w; x++) {
            g_backend->put_char(g_backend->impl_data, x, y, g_terminal.data[y * g_terminal.w + x], COL_BACKGROUND, COL_FOREGROUND);
        }
    }
}

static void _term_putchar(char c)
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
        g_backend->put_char(g_backend->impl_data, g_terminal.cursor.x, g_terminal.cursor.y, ' ', COL_BACKGROUND, COL_FOREGROUND);
        break;
    case '\t': {
        int spaces = g_terminal.cursor.x % TAB_SIZE == 0 ? TAB_SIZE : TAB_SIZE - (g_terminal.cursor.x % 4);
        for (int i = 0; i < spaces; i++)
            putchar(' ');
        return;
    }
    default:
        g_terminal.data[g_terminal.cursor.y * g_terminal.w + g_terminal.cursor.x] = c;
        g_backend->put_char(g_backend->impl_data, g_terminal.cursor.x, g_terminal.cursor.y, c, COL_BACKGROUND, COL_FOREGROUND);
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
        term_redraw_all();
    }
}

static int term_stdout_stderr_override(char const *s, int len)
{
    for (int i = 0; i < len; i++)
        _term_putchar(s[i]);
    term_refresh();
    return len;
}

static KeyEvent wait_for_key_event(void)
{
    KeyEvent event;
    event = (KeyEvent) {1, 2, 3};
    while (syscall(SYS_PollInput, NULL, 0, (uint32_t) &event, 0, 0, 0) != 0)
        ;
    return event;
}

void terminal_init(void)
{
    gfx_backend_init(&g_backend);

    g_terminal.cursor.x = 0;
    g_terminal.cursor.y = 0;
    g_terminal.w = g_backend->get_width(g_backend->impl_data);
    g_terminal.h = g_backend->get_height(g_backend->impl_data);
    g_terminal.data = malloc(g_terminal.w * g_terminal.h);
    memset(g_terminal.data, ' ', g_terminal.w * g_terminal.h);
    term_redraw_all();
    term_refresh();

    // s_stdout_print_func = term_stdout_stderr_override;
    // s_stderr_print_func = term_stdout_stderr_override;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

char *read_line(const char *prompt)
{
    printf("%s", prompt);

    char *line = malloc(8);
    size_t length = 0, allocated = 8;
    
    do {
        KeyEvent event = wait_for_key_event();
        if (event.press_state == false)
            continue;
        
        if (length > 0 && (event.character == '\n' || event.character == '\r'))
            break;

        if (length == allocated - 1) {
            allocated += 8;
            line = realloc(line, allocated);
        }
        line[length++] = event.character;
        line[length] = '\0';
        printf("%c", event.character);
    } while (true);
    putchar('\n');

    return line;
}
