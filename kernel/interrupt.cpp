#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/lib/memory.h>

namespace kernel {

static void (*swi_handler[256])(VectorFrame*) = { 0 };

void software_interrupt_handler(VectorFrame* frame)
{
    auto swi_number = *reinterpret_cast<uint32_t*>(frame->lr_mode - 4) & 0xff;

    if (swi_handler[swi_number] == nullptr) {
        kprintf("Unknown software interrupt: %d\n", swi_number);
        return;
    }

    swi_handler[swi_number](frame);
}

void install_software_interrupt_handler(uint32_t swi_number, void (*handler)(struct VectorFrame*))
{
    if (swi_number > 255) {
        panic("Cannot install software interrupt with number %d\n", swi_number);
        return;
    }
    if (swi_handler[swi_number] != nullptr) {
        panic("Software interrupt with number %d already installed\n", swi_number);
        return;
    }

    swi_handler[swi_number] = handler;
}

void irq_handler(VectorFrame*)
{
    kprintf("IRQ HANDLER\n");
    while (1)
        ;
}

// This gets called by the assembly code in vector_table.S
extern "C" void irq_and_exception_handler(uint32_t vector_offset, VectorFrame* frame)
{
    char const* vector_name[] = {
        "RESET",
        "UNDEFINED INSTRUCTION",
        "SOFTWARE INTERRUPT",
        "PREFETCH ABORT",
        "DATA ABORT",
        "UNUSED",
        "IRQ",
        "FIQ"
    };
    auto vector_index = vector_offset / 4;

    if (vector_index > klib::array_size(vector_name))
        panic("UNEXPECTED VECTOR OFFSET: %x\n", vector_offset);

    if (vector_index == 2) {
        software_interrupt_handler(frame);
    } else if (vector_index == 6) {
        irq_handler(frame);
    } else {
        panic("%s caused by instruction at %p\n", vector_name[vector_index], frame->lr_mode);
    }
}

}
