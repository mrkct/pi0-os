#pragma once

#ifndef UNIT_TEST

#include <kernel/base.h>


typedef void (*PutsFunc)(const char *c, size_t size);

void kprintf_set_puts_func(PutsFunc f);

size_t kprintf(char const* format, ...);

#else

#include <stdio.h>
#include <stdarg.h>

static inline size_t kprintf(char const *format, ...)
{
    va_list args;
    va_start(args, format);
    size_t result = vprintf(format, args);
    va_end(args);
    return result;
}

#endif
