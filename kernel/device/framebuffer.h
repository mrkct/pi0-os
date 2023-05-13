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

}
