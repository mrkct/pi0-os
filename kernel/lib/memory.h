#pragma once

#include <stddef.h>
#include <stdint.h>

namespace klib {

void kmemcpy(void* dest, void const* src, size_t n);

void kmemset(void* s, uint8_t c, size_t n);

}

using klib::kmemcpy;
using klib::kmemset;
