#pragma once

#include <stddef.h>
#include <stdint.h>


extern "C" void* memset(void* s, int c, size_t n);

extern "C" void* memcpy(void* dest, void const* src, size_t n);

extern "C" int memcmp(const void *s1, const void *s2, size_t n);

extern "C" char *strcpy(char *dst, const char *src);

extern "C" size_t strnlen(const char *s, size_t maxlen);

constexpr char *constexpr_strcpy(char *dst, const char *src)
{
    char *ret_dst = dst;
    while (*src) {
        *dst = *src;
        dst++;
        src++;
    }
    *dst = '\0';

    return ret_dst;
}

extern "C" char* strncpy_safe(char* dest, char const* src, size_t n);

extern "C" int strcmp(char const* s1, char const* s2);

extern "C" size_t strlen(char const* s);

extern "C" char tolower(char c);
