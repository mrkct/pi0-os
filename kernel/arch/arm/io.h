#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel/base.h>


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

template<typename Callback>
Error retry_with_timeout(Callback callback)
{
    constexpr uint32_t TIMEOUT = 1000000;
    for (uint32_t i = 0; i < TIMEOUT; ++i) {
        if (callback())
            return Success;

        wait_cycles(500);
    }

    return ResponseTimeout;
}
