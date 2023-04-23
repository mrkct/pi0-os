#pragma once

#include <stdint.h>

namespace kernel {

static constexpr uintptr_t MMIO_BASE = 0x3F000000;

static inline constexpr uintptr_t bcm2835_bus_address_to_physical(uintptr_t addr)
{
    return addr - 0x7e000000 + 0x20000000;
}

static inline constexpr uintptr_t videocore_address_to_physical(uintptr_t addr)
{
    return addr + 0x20000000;
}

template<typename T>
static inline T ioread32(uintptr_t reg)
{
    static_assert(sizeof(T) == 4, "ioread32 can only read 32-bit values but the template parameter is not 32-bit wide");
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
    union {
        uint32_t u32;
        T t;
    } u;
    u.t = data;
    *reinterpret_cast<uint32_t volatile*>(reg) = u.u32;
}

static inline void wait_cycles(uint32_t cycles)
{
    asm volatile("1: subs %[cycles], %[cycles], #1; bne 1b"
                 : [cycles] "+r"(cycles));
}

static inline void memory_barrier()
{
    // TODO: Need more research on this
}

}
