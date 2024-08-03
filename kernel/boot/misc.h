#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <kernel/sizes.h>
#include <kernel/lib/math.h>
#include "board/board.h"



#define EARLY_ASSERT(condition)                                     \
    do {                                                            \
        if (!(condition)) {                                         \
            early_kprintf("Assertion failed: %s\n", #condition);    \
            while (1);                                              \
        }                                                           \
    } while(0)

void bootmem_init(uintptr_t start, uintptr_t size);

void *bootmem_alloc(size_t size, size_t alignment);

size_t bootmem_allocated(void);

size_t early_kprintf(char const* format, ...);
