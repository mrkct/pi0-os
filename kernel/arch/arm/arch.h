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

static inline uint8_t ioread8(uintptr_t reg)
{
    memory_barrier();
    return *reinterpret_cast<volatile uint8_t*>(reg);
}

static inline uint16_t ioread16(uintptr_t reg)
{
    memory_barrier();
    return *reinterpret_cast<volatile uint16_t*>(reg);
}

static inline uint32_t ioread32(uintptr_t reg)
{
    memory_barrier();
    return *reinterpret_cast<uint32_t volatile*>(reg);
}

static inline uint8_t ioread8(volatile void *reg) { return ioread8(reinterpret_cast<uintptr_t>(reg)); }
static inline uint16_t ioread16(volatile void *reg) { return ioread16(reinterpret_cast<uintptr_t>(reg)); }
static inline uint32_t ioread32(volatile void *reg) { return ioread32(reinterpret_cast<uintptr_t>(reg)); }

static inline void iowrite8(uintptr_t reg, uint8_t value)
{
    memory_barrier();
    *reinterpret_cast<uint8_t volatile*>(reg) = value;
}

static inline void iowrite16(uintptr_t reg, uint16_t value)
{
    memory_barrier();
    *reinterpret_cast<uint16_t volatile*>(reg) = value;
}

static inline void iowrite32(uintptr_t reg, uint32_t data)
{
    memory_barrier();
    *reinterpret_cast<uint32_t volatile*>(reg) = data;
}

static inline void iowrite8(volatile void *reg, uint8_t value) { iowrite8(reinterpret_cast<uintptr_t>(reg), value); }
static inline void iowrite16(volatile void *reg, uint16_t value) { iowrite16(reinterpret_cast<uintptr_t>(reg), value); }
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


/**
 * \brief Move to Coprocessor from ARM core register
 * Inline-assembly for the following instruction:
 *  MCR<c> <coproc>, <opc1>, <Rt>, <CRn>, <CRm>{, <opc2>}
 * 
 * Where 'value' is loaded into a register and passed as Rt
*/
#define ARM_MCR(coproc, opc1, value, CRn, CRm, opc2)                                \
    do {                                                                            \
        asm volatile(                                                               \
            "mcr " #coproc ", " #opc1 ", %0, " #CRn ", " #CRm ", " #opc2 "\n"       \
            :                                                                       \
            : "r"(value)                                                            \
            : "memory"                                                              \
        );                                                                          \
    } while(0)

/**
 * \brief Move to Coprocessor from two ARM core registers
 * Inline-assembly for the following instruction:
 *  MCRR<c> <coproc>, <opc1>, <Rt>, <Rt2>, <CRm>
 * 
 * Where 'value' is loaded into a register and passed as Rt
*/
#define ARM_MCRR(coproc, opc1, value1, value2, CRm)                                 \
    do {                                                                            \
        asm volatile(                                                               \
            "mcrr " #coproc ", " #opc1 ", %0, %1, " #CRm "\n"                       \
            :                                                                       \
            : "r"(value1), "r"(value2)                                              \
            : "memory"                                                              \
        );                                                                          \
    } while(0)


/**
 * \brief Move to ARM core register from Coprocessor
 * Inline-assembly for the following instruction:
 *  MRC<c> <coproc>, <opc1>, <Rt>, <CRn>, <CRm>{, <opc2>}
 * 
 * Where 'value' is loaded into a register and passed as Rt
*/
#define ARM_MRC(coproc, opc1, value, CRn, CRm, opc2)                                \
    do {                                                                            \
        asm volatile(                                                               \
            "mrc " #coproc ", " #opc1 ", %0, " #CRn ", " #CRm ", " #opc2 "\n"       \
            : "=r"(value)                                                           \
            :                                                                       \
            : "memory"                                                              \
        );                                                                          \
    } while(0)

/**
 * \brief Move to two ARM core registers from Coprocessor
 * Inline-assembly for the following instruction:
 *  MRRC<c> <coproc>, <opc>, <Rt>, <Rt2>, <CRm>
 * 
 * Where 'value1' and 'value2' are where Rt and Rt2 will be stored
*/
#define ARM_MRRC(coproc, opc1, value1, value2, CRm)                                 \
    do {                                                                            \
        asm volatile(                                                               \
            "mrrc " #coproc ", " #opc1 ", %0, %1, " #CRm "\n"                       \
            : "=r"(value1), "=r"(value2)                                            \
            :                                                                       \
            : "memory"                                                              \
        );                                                                          \
    } while(0)

static inline uint32_t read_cpsr()
{
    uint32_t cpsr;
    asm volatile("mrs %0, cpsr" : "=r" (cpsr));
    return cpsr;
}

static inline bool is_supervisor_mode()
{
    uint32_t cpsr = read_cpsr();
    return (cpsr & 0x1F) == 0x13;
}
