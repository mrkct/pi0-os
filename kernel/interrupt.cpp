#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>

namespace kernel {

static void (*swi_handler[256])(struct InterruptFrame*) = { 0 };

extern "C" void software_interrupt_handler_c(InterruptFrame* frame)
{
    auto swi_number = *reinterpret_cast<uint32_t*>(frame->lr - 4) & 0xff;

    if (swi_handler[swi_number] == nullptr) {
        kprintf("Unknown software interrupt: %d\n", swi_number);
        return;
    }

    swi_handler[swi_number](frame);
}

void install_software_interrupt(uint32_t syscall_number, void (*handler)(struct InterruptFrame*))
{
    if (syscall_number > 255) {
        panic("Cannot install software interrupt with number %d\n", syscall_number);
        return;
    }
    if (swi_handler[syscall_number] != nullptr) {
        panic("Software interrupt with number %d already installed\n", syscall_number);
        return;
    }

    swi_handler[syscall_number] = handler;
}

}
