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

void init_interrupts();

void enable_interrupts();
void disable_interrupts();

void install_software_interrupt_handler(uint32_t swi_number, void (*handler)(struct VectorFrame*));

}
