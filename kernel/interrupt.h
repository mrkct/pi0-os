#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct InterruptFrame {
    uint32_t stack_alignment_padding;
    uint32_t lr;
    uint32_t r[8];
};

void init_interrupts();

void enable_interrupts();
void disable_interrupts();

void install_software_interrupt(uint32_t syscall_number, void (*handler)(struct InterruptFrame*));

}
