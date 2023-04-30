#pragma once

#include <kernel/device/videocore.h>
#include <kernel/error.h>
#include <kernel/lib/psf/psf.h>

namespace kernel {

static constexpr size_t CONSOLE_WIDTH = 80;
static constexpr size_t CONSOLE_HEIGHT = 24;
static constexpr size_t TAB_SIZE = 4;

struct VideoConsole {
    klib::PSFFont font;
    Framebuffer fb;
    size_t offset_x, offset_y;
    size_t cursor_x, cursor_y;
    uint16_t data[CONSOLE_WIDTH * CONSOLE_HEIGHT];
};

Error videoconsole_init(VideoConsole&, Framebuffer, size_t offset_x = 0, size_t offset_y = 0);

void videoconsole_putc(VideoConsole&, char c);

void videoconsole_clear(VideoConsole&);

}
