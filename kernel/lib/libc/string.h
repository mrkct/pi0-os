#pragma once

#include <stddef.h>

extern "C" void* memset(void* s, int c, size_t n);

extern "C" void* memcpy(void* dest, void const* src, size_t n);
