#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct Area {
    uintptr_t start;
    uintptr_t end;

    constexpr bool contains(uintptr_t ptr) const
    {
        return start <= ptr && ptr < end;
    }
};

namespace areas {

static constexpr Area higher_half = { 0xe0000000, 0xffffffff };

extern "C" uint8_t _kernel_stack_start[];
extern "C" uint8_t _kernel_stack_end[];
static const Area kernel_stack = { reinterpret_cast<uintptr_t>(_kernel_stack_start), reinterpret_cast<uintptr_t>(_kernel_stack_end) };

static constexpr Area kernel = { 0xe0000000, 0xe2000000 };
static constexpr Area peripherals = { 0xe2000000, 0xe3000000 };
static constexpr Area framebuffer = { 0xe3000000, 0xe4000000 };
static constexpr Area temp_mappings = { 0xe4000000, 0xe4100000 };
static constexpr Area heap = { 0xe4100000, 0xffffffff };

}

}
