#pragma once

#include <api/arm/syscall.h>


struct InterruptFrame {
    uint32_t task_lr;
    uint32_t task_sp;
    uint32_t r[13];
    uint32_t lr;
    uint32_t spsr;

    void set_syscall_return_value(uint32_t value) { r[0] = value; }
};

void arch_irq_init();

static inline bool arch_irq_enabled()
{
    uint32_t cpsr;
    asm volatile("mrs %0, cpsr"
                 : "=r"(cpsr));
    return !(cpsr & (1 << 7));
}

static inline void arch_irq_enable() { asm volatile("cpsie i"); }

static inline void arch_irq_disable() { asm volatile("cpsid i"); }
