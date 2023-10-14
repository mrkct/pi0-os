#pragma once

#include <kernel/error.h>
#include <kernel/memory/areas.h>
#include <stdint.h>

namespace kernel {

static inline constexpr uintptr_t bcm2835_bus_address_to_physical(uintptr_t addr)
{
    return areas::peripherals.start + (addr - 0x7e000000);
}

static inline constexpr uintptr_t videocore_address_to_physical(uintptr_t addr)
{
    return areas::peripherals.start + addr;
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

template<typename T>
static inline T ioread32(uintptr_t reg)
{
    static_assert(sizeof(T) == 4, "ioread32 can only read 32-bit values but the template parameter is not 32-bit wide");
    if (!areas::peripherals.contains(reg))
        panic("ioread32: address %p is not in the peripherals area", reg);

    memory_barrier();
    union {
        uint32_t u32;
        T t;
    } u;
    u.u32 = *reinterpret_cast<uint32_t volatile*>(reg);
    return u.t;
}

template<typename T>
static inline void iowrite32(uintptr_t reg, T data)
{
    static_assert(sizeof(T) == 4, "iowrite32 can only write 32-bit values but the template parameter is not 32-bit wide");
    if (!areas::peripherals.contains(reg))
        panic("iowrite32: address %p is not in the peripherals area", reg);

    union {
        uint32_t u32;
        T t;
    } u;
    memory_barrier();
    u.t = data;
    *reinterpret_cast<uint32_t volatile*>(reg) = u.u32;
}

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

}
