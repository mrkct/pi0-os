#pragma once

#include <api/arm/syscall.h>


struct InterruptFrame {
    uint32_t user_sp;           // The 'sp' register saved from 'User/System' mode
    uint32_t user_lr;           // The  lr register saved from 'User/System' mode
    uint32_t r[13];             // The r0-r12 registers
    uint32_t supervisor_lr;     // The lr register saved from 'Supervisor' mode
    uint32_t lr;                // Address that will be returned to after the trap
    uint32_t spsr;              // "Saved Program Status Register", pushed immediately at trap entrys

    void set_syscall_return_value(uint32_t value) { r[0] = value; }
};

struct ContextSwitchFrame {
    uint32_t r[13];             // The r0-r12 registers, used by the kernel at the time of the context switch
    uint32_t lr;
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

void arch_context_switch(ContextSwitchFrame **from, ContextSwitchFrame *to);

void arch_create_initial_kernel_stack(
    void **kernel_stack_ptr,
    InterruptFrame **iframe,
    uintptr_t userstack,
    uintptr_t entrypoint,
    bool privileged
);
