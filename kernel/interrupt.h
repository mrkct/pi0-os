#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/arch/arch.h>


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

void interrupt_install_swi_handler(uint32_t swi_number, InterruptHandler);
void interrupt_install_basic_irq_handler(uint32_t irq_number, InterruptHandler);
void interrupt_install_irq1_handler(uint32_t irq_number, InterruptHandler);
void interrupt_install_irq2_handler(uint32_t irq_number, InterruptHandler);
