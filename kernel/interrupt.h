#pragma once

#include <stddef.h>
#include <stdint.h>


enum class InterruptVector: int {
    Reset = 0,
    UndefinedInstruction,
    SoftwareInterrupt,
    PrefetchAbort,
    DataAbort,
    Unused,
    IRQ,
    FIQ
};

struct SuspendedTaskState {
    uint32_t task_lr;
    uint32_t task_sp;
    uint32_t r[13];
    uint32_t lr;
    uint32_t spsr;
};

struct VectorFrame {
    uint32_t stack_alignment_padding;
    SuspendedTaskState* suspended_state;
};

typedef void (*InterruptHandler)(SuspendedTaskState*);

void interrupt_init();

static inline bool interrupt_are_enabled()
{
    uint32_t cpsr;
    asm volatile("mrs %0, cpsr"
                 : "=r"(cpsr));
    return !(cpsr & (1 << 7));
}
static inline void interrupt_enable() { asm volatile("cpsie i"); }
static inline void interrupt_disable() { asm volatile("cpsid i"); }

void interrupt_install_swi_handler(uint32_t swi_number, InterruptHandler);
void interrupt_install_basic_irq_handler(uint32_t irq_number, InterruptHandler);
void interrupt_install_irq1_handler(uint32_t irq_number, InterruptHandler);
void interrupt_install_irq2_handler(uint32_t irq_number, InterruptHandler);
