#pragma once

#include <stdint.h>
#include <stddef.h>
#include "armirq.h"


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

static inline uint32_t ioread32(uintptr_t reg)
{
    memory_barrier();
    return *reinterpret_cast<uint32_t volatile*>(reg);
}

static inline uint32_t ioread32(volatile void *reg) { return ioread32(reinterpret_cast<uintptr_t>(reg)); }

static inline void iowrite32(uintptr_t reg, uint32_t data)
{
    memory_barrier();
    *reinterpret_cast<uint32_t volatile*>(reg) = data;
}

static inline void iowrite32(volatile void *reg, uint32_t data) { iowrite32(reinterpret_cast<uintptr_t>(reg), data); }

static inline void wait_cycles(uint32_t cycles)
{
    // FIXME: This is passing cycles as an output operand, which is not correct
    asm volatile("1: subs %[cycles], %[cycles], #1; bne 1b"
                 : [cycles] "+r"(cycles));
}

static inline bool try_acquire(uint32_t *lock)
{
    uint32_t old_value = 0;
    asm volatile(
        "mov r0, #1\n"
        "swp %0, r0, [%1]\n"
        : "=r&"(old_value)
        : "r"(lock)
        : "r0");

    return old_value == 0;
}

static inline void cpu_relax()
{
    asm volatile("wfe");
}
