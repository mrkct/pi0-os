#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "doomgeneric.h"
#include "doomkeys.h"

#include <api/syscalls.h>
#define _UAPI_INPUT_EVENT_CODES_H /* Prevent inclusion of the #defines for the input device */
#include <api/input.h>

/* 
 * These are the same exact defines from <api/input.h>, except with a different name because
 * the header's names clash with the defines from Doom
 */
#define EV_KEY			    0x01
#define OS_KEY_ESC          1
#define OS_KEY_ENTER        28
#define OS_KEY_J		    36
#define OS_KEY_LEFT 	    105
#define OS_KEY_RIGHT        106
#define OS_KEY_UP   	    103
#define OS_KEY_DOWN 	    108
#define OS_KEY_L		    38
#define OS_KEY_I		    23
#define OS_KEY_LEFTALT	    56
#define OS_KEY_RIGHTALT	    100
#define OS_KEY_SPACE    	57
#define OS_KEY_LEFTCTRL 	29
#define OS_KEY_RIGHTCTRL    97
#define OS_KEY_R    		19
#define OS_KEY_K    		37
#define OS_KEY_LEFTSHIFT		42
#define OS_KEY_RIGHTSHIFT		54

static struct {
    int fd;
    FramebufferDisplayInfo info;
    uint8_t *addr;
} s_fb = {};

static struct {
    int fd;
    enum { KEYBOARD } type;
} s_input;


static int open_framebuffer()
{
    int rc = 0;

    s_fb.fd = sys_open("/dev/framebuffer0", OF_RDWR, 0);
    if (s_fb.fd < 0) {
        fprintf(stderr, "Failed to open framebuffer\n");
        return -1;
    }

    if (0 != (rc = sys_ioctl(s_fb.fd, FBIO_GET_DISPLAY_INFO, &s_fb.info))) {
        fprintf(stderr, "sys_ioctl(FBIO_GET_DISPLAY_INFO) failed\n");
        goto cleanup;
    }

    // This is completely arbitrary...
    s_fb.addr = (uint8_t*) 0x80000000;
    if (0 != (rc = sys_mmap(s_fb.fd, s_fb.addr, s_fb.info.pitch * s_fb.info.height, 0))) {
        fprintf(stderr, "sys_ioctl(FBIO_MAP) failed\n");
        goto cleanup;
    }

    return 0;

cleanup:
    return rc;
}

static int open_input()
{
    s_input.fd = sys_open("/dev/input0", OF_RDONLY | OF_NONBLOCK, 0);
    if (s_input.fd < 0) {
        fprintf(stderr, "Failed to open /dev/input0\n");
        return -1;
    }

    s_input.type = KEYBOARD;
    return 0;
}

static bool read_input_event(InputEvent *out_evt)
{
    int rc;
retry:
    rc = read(s_input.fd, out_evt, sizeof(InputEvent));
    if (rc < 0) {
        fprintf(stderr, "Input event fd broke: %d\n", rc);
        sys_exit(-1);
    }

    if (rc == sizeof(InputEvent) && out_evt->type != EV_KEY) {
        goto retry;
    }

    return rc == sizeof(InputEvent);
}

void DG_Init()
{
    int rc = 0;
    rc = open_framebuffer();
    if (rc < 0) {
        fprintf(stderr, "Failed to open framebuffer: %d\n", rc);
        exit(-1);
    }

    rc = open_input();
    if (rc < 0) {
        fprintf(stderr, "Failed to open input: %d\n", rc);
        exit(-1);
    }
}

void DG_DrawFrame()
{
    for (size_t y = 0; y < DOOMGENERIC_RESY; y++) {
        memcpy(
            (uint8_t*) s_fb.addr + y * s_fb.info.pitch, 
            (uint8_t*) DG_ScreenBuffer + y * DOOMGENERIC_RESX * sizeof(uint32_t), 
            DOOMGENERIC_RESX * sizeof(uint32_t)
        );
    }
    sys_ioctl(s_fb.fd, FBIO_REFRESH, 0);
}

void DG_SleepMs(uint32_t ms)
{
    sys_millisleep(ms);
}

uint32_t DG_GetTicksMs()
{
    return sys_getticks();
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    InputEvent event;
    if (!read_input_event(&event))
        return 0;

    *pressed = event.key.pressed;
    switch (event.key.code) {
    #define MAP(os, doom) case os: { *doomKey = doom; return 1; }
        MAP(OS_KEY_ENTER, KEY_ENTER)
        MAP(OS_KEY_ESC, KEY_ESCAPE)

        MAP(OS_KEY_J, KEY_LEFTARROW)
        MAP(OS_KEY_LEFT, KEY_LEFTARROW)

        MAP(OS_KEY_L, KEY_RIGHTARROW)
        MAP(OS_KEY_RIGHT, KEY_RIGHTARROW)
    
        MAP(OS_KEY_I, KEY_UPARROW)
        MAP(OS_KEY_UP, KEY_UPARROW)
    
        MAP(OS_KEY_K, KEY_DOWNARROW)
        MAP(OS_KEY_DOWN, KEY_DOWNARROW)
    
        MAP(OS_KEY_LEFTCTRL, KEY_FIRE)
        MAP(OS_KEY_SPACE, KEY_USE)
        
        MAP(OS_KEY_LEFTSHIFT, KEY_RSHIFT)
        MAP(OS_KEY_RIGHTSHIFT, KEY_RSHIFT)
        
        MAP(OS_KEY_LEFTALT, KEY_LALT)
        MAP(OS_KEY_RIGHTALT, KEY_RALT)
        default:
            *pressed = 0;
            return 0;
    }
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
        DG_SleepMs(1000 / 60);
    }

    return 0;
}
