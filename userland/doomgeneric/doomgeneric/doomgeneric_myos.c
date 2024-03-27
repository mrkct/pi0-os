#include <api/syscalls.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "libgfx.h"
#include "libdatetime.h"

#include "doomgeneric.h"
#include "doomkeys.h"


static Window window;
static int32_t ticks_fd;

void DG_Init()
{
    window = open_window(
        "DOOM",
        DOOMGENERIC_RESX,
        DOOMGENERIC_RESY,
        false);
    ticks_fd = open("/sys/time", O_RDONLY);
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
    return 0;
}

void DG_SetWindowTitle(char const* title)
{
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
