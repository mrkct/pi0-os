#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct VectorFrame {
    uint32_t stack_alignment_padding;
    uint32_t lr;
    uint32_t r[13];
    uint32_t lr_mode, spsr_mode;
};

typedef void (*InterruptHandler)(VectorFrame*);

void interrupt_init();

static inline void interrupt_enable() { asm volatile("cpsie i"); }
static inline void interrupt_disable() { asm volatile("cpsid i"); }

void interrupt_install_swi_handler(uint32_t swi_number, InterruptHandler);
void interrupt_install_basic_irq_handler(uint32_t irq_number, InterruptHandler);
void interrupt_install_irq1_handler(uint32_t irq_number, InterruptHandler);
void interrupt_install_irq2_handler(uint32_t irq_number, InterruptHandler);

}
