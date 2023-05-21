#pragma once

#include <stddef.h>
#include <stdint.h>

namespace klib {

char* strncpy_safe(char* dest, char const* src, size_t n);

int strcmp(char const* s1, char const* s2);

size_t strlen(char const* s);

}
