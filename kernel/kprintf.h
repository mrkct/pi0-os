#pragma once

#include <kernel/base.h>


typedef void (*PutCharFunc)(char c);

void kprintf_set_putchar_func(PutCharFunc f);

size_t kprintf(char const* format, ...);

