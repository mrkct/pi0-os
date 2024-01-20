#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct Framebuffer {
    size_t width, height;
    size_t pitch, depth;
    uintptr_t physical_address;
    uint32_t* address;
    size_t size;
};

Framebuffer& get_main_framebuffer();

void blit_to_main_framebuffer(uint32_t* data, int32_t x, int32_t y, uint32_t width, uint32_t height);

}
