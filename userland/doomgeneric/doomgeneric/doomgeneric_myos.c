#include <api/syscalls.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "libgfx.h"
#include "libdatetime.h"
#include "api/input.h"
#include "doomgeneric.h"
#include "doomkeys.h"


static Window window;
static int32_t ticks_fd, kbd_fd;
static struct {
#define KEYQUEUE_SIZE 16
    KeyEvent data[KEYQUEUE_SIZE];
    size_t size;
} key_events;

static void refill_key_events()
{
    int bytes_read = read(kbd_fd, key_events.data, sizeof(KeyEvent) * KEYQUEUE_SIZE);
    if (bytes_read < 0)
        return;
    key_events.size = bytes_read / sizeof(KeyEvent);
}

static int get_key_event(KeyEvent *key)
{
    if (key_events.size == 0)
        return -1;
    
    *key = key_events.data[0];
    memmove(key_events.data, key_events.data + 1, sizeof(KeyEvent) * (key_events.size - 1));
    key_events.size--;
    return 0;
}

static unsigned char keyevent_to_doomkey(KeyEvent *key)
{
    switch (key->keycode) {
    case KEYCODE_RET: return KEY_ENTER;
    case KEYCODE_ESC: return KEY_ESCAPE;

    case KEYCODE_J:
    case KEYCODE_LEFT: return KEY_LEFTARROW;
    
    case KEYCODE_L:
    case KEYCODE_RIGHT: return KEY_RIGHTARROW;
    
    case KEYCODE_I:
    case KEYCODE_UP: return KEY_UPARROW;
    
    case KEYCODE_K:
    case KEYCODE_DOWN: return KEY_DOWNARROW;
    
    case KEYCODE_CTRL: return KEY_FIRE;
    case KEYCODE_SPC: return KEY_USE;
    case KEYCODE_SHIFT:
    case KEYCODE_SHIFT_R: return KEY_RSHIFT;
    case KEYCODE_ALT:
    case KEYCODE_ALT_R: return KEY_RALT;
    default:
        return tolower(key->character);
    }
}

void DG_Init()
{
    window = open_window(
        "DOOM",
        DOOMGENERIC_RESX,
        DOOMGENERIC_RESY,
        true);
    ticks_fd = open("/sys/time", O_RDONLY);
    kbd_fd = open("/sys/kbd", 0, O_RDONLY);
}

void DG_DrawFrame()
{
    memcpy(window.framebuffer, DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint32_t));
    refresh_window(&window);
}

void DG_SleepMs(uint32_t ms)
{
    os_sleep(ms);
}

uint32_t DG_GetTicksMs()
{
    uint64_t ticks;
    read(ticks_fd, &ticks, sizeof(uint64_t));
    return (uint32_t) ticks;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    if (key_events.size == 0)
        refill_key_events();

    KeyEvent event;
    if (0 != get_key_event(&event))
        return 0;
    
    *pressed = event.pressed;
    *doomKey = keyevent_to_doomkey(&event);
    return 1;
}

void DG_SetWindowTitle(char const* title)
{
    (void) title;
}

int main(int argc, char** argv)
{
    doomgeneric_Create(argc, argv);

    for (int i = 0;; i++) {
        doomgeneric_Tick();
        DG_SleepMs(1000 / 250);
    }

    return 0;
}
