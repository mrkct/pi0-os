#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <kernel/sizes.h>
#include <kernel/lib/more_math.h>
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

static inline void delay(uint32_t ticks)
{
    while (ticks--);
}

static inline void memory_barrier()
{
    asm volatile(
        "mcr p15, 0, r3, c7, c5, 0  \n" // Invalidate instruction cache
        "mcr p15, 0, r3, c7, c5, 6  \n" // Invalidate BTB
        "mcr p15, 0, r3, c7, c10, 4 \n" // Drain write buffer
        "mcr p15, 0, r3, c7, c5, 4  \n" // Prefetch flush
        :
        :
        : "r3");
}

static inline void iowrite32(uintptr_t reg, uint32_t data)
{
    memory_barrier();
    *reinterpret_cast<uint32_t volatile*>(reg) = data;
}

static inline uint32_t ioread32(uintptr_t reg)
{
    memory_barrier();
    return *reinterpret_cast<uint32_t volatile*>(reg);
}
