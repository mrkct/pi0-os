#pragma once

#include <stddef.h>
#include <stdint.h>

namespace klib {

char* strncpy_safe(char* dest, char const* src, size_t n);

}
