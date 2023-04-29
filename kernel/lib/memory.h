#pragma once

#include <stddef.h>
#include <stdint.h>

namespace klib {

void kmemcpy(void* dest, void const* src, size_t n);

void kmemset(void* s, uint8_t c, size_t n);

template<typename T, size_t N>
constexpr size_t array_size(T (&)[N]) { return N; }

}

using klib::kmemcpy;
using klib::kmemset;
