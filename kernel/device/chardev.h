#pragma once

#include <kernel/error.h>
#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct CharacterDevice {
    Error (*init)(void*);
    Error (*read)(void*, uint8_t&);
    Error (*write)(void*, uint8_t);
    void* data;
};

}
