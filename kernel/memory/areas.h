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

extern "C" uint8_t __kernel_stack_start[];
extern "C" uint8_t __kernel_stack_end[];
static const Area kernel_stack = { reinterpret_cast<uintptr_t>(__kernel_stack_start), reinterpret_cast<uintptr_t>(__kernel_stack_end) };

static constexpr Area peripherals = { 0x20000000, 0x21000000 };

}

}
