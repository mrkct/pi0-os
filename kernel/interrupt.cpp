#include <kernel/device/io.h>
#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/lib/memory.h>

namespace kernel {

static InterruptHandler g_swi_handlers[256] = { nullptr };
static InterruptHandler g_basic_irq_handlers[32] = { nullptr };
static InterruptHandler g_irq1_handlers[32] = { nullptr };
static InterruptHandler g_irq2_handlers[32] = { nullptr };

static void software_interrupt_handler(VectorFrame* frame)
{
    auto swi_number = *reinterpret_cast<uint32_t*>(frame->lr_mode - 4) & 0xff;

    if (g_swi_handlers[swi_number] == nullptr) {
        kprintf("Unknown software interrupt: %d\n", swi_number);
        return;
    }

    g_swi_handlers[swi_number](frame);
}

static constexpr uintptr_t IRQ_CONTROLLER_BASE = bcm2835_bus_address_to_physical(0x7E00B000);

static constexpr uintptr_t IRQ_BASIC_PENDING = IRQ_CONTROLLER_BASE + 0x200;
static constexpr uintptr_t IRQ_PENDING_1 = IRQ_CONTROLLER_BASE + 0x204;
static constexpr uintptr_t IRQ_PENDING_2 = IRQ_CONTROLLER_BASE + 0x208;
static constexpr uintptr_t FIQ_CONTROL = IRQ_CONTROLLER_BASE + 0x20c;
static constexpr uintptr_t ENABLE_IRQS_1 = IRQ_CONTROLLER_BASE + 0x210;
static constexpr uintptr_t ENABLE_IRQS_2 = IRQ_CONTROLLER_BASE + 0x214;
static constexpr uintptr_t ENABLE_BASIC_IRQS = IRQ_CONTROLLER_BASE + 0x218;
static constexpr uintptr_t DISABLE_IRQS_1 = IRQ_CONTROLLER_BASE + 0x21c;
static constexpr uintptr_t DISABLE_IRQS_2 = IRQ_CONTROLLER_BASE + 0x220;
static constexpr uintptr_t DISABLE_BASIC_IRQS = IRQ_CONTROLLER_BASE + 0x224;

static void irq_handler(VectorFrame* frame)
{
    uint32_t basic, pending1, pending2;
    basic = ioread32<uint32_t>(IRQ_BASIC_PENDING);
    pending1 = ioread32<uint32_t>(IRQ_PENDING_1);
    pending2 = ioread32<uint32_t>(IRQ_PENDING_2);

    static constexpr uint32_t ONE_OR_MORE_BITS_SET_IN_PENDING1 = 1 << 8;
    static constexpr uint32_t ONE_OR_MORE_BITS_SET_IN_PENDING2 = 1 << 9;

    bool also_check_pending1 = basic & ONE_OR_MORE_BITS_SET_IN_PENDING1;
    bool also_check_pending2 = basic & ONE_OR_MORE_BITS_SET_IN_PENDING2;

    for (int i = 0; i < 32; ++i) {
        auto mask = 1 << i;
        if (mask == ONE_OR_MORE_BITS_SET_IN_PENDING1 || mask == ONE_OR_MORE_BITS_SET_IN_PENDING2)
            continue;

        if (basic & mask) {
            if (g_basic_irq_handlers[i] == nullptr)
                panic("Unhandled IRQ %d\n", i);

            g_basic_irq_handlers[i](frame);
        }
    }

    if (also_check_pending1) {
        for (int i = 0; i < 32; ++i) {
            auto mask = 1 << i;

            if (pending1 & mask) {
                if (g_irq1_handlers[i] == nullptr)
                    panic("Unhandled IRQ1 %d\n", i);

                g_irq1_handlers[i](frame);
            }
        }
    }

    if (also_check_pending2) {
        for (int i = 0; i < 32; ++i) {
            auto mask = 1 << i;

            if (pending2 & mask) {
                if (g_irq2_handlers[i] == nullptr)
                    panic("Unhandled IRQ2 %d\n", i);

                g_irq2_handlers[i](frame);
            }
        }
    }
}

void interrupt_init()
{
    iowrite32(DISABLE_IRQS_1, 0xffffffff);
    iowrite32(DISABLE_IRQS_2, 0xffffffff);
    iowrite32(DISABLE_BASIC_IRQS, 0xffffffff);
}

void interrupt_install_swi_handler(uint32_t swi_number, InterruptHandler handler)
{
    if (swi_number > 255) {
        panic("Cannot install software interrupt with number %d\n", swi_number);
        return;
    }
    if (g_swi_handlers[swi_number] != nullptr) {
        panic("Software interrupt with number %d already installed\n", swi_number);
        return;
    }

    g_swi_handlers[swi_number] = handler;
}

static void install_irq(
    char const* name,
    uint32_t irq_number,
    InterruptHandler handler,
    InterruptHandler handlers[],
    uintptr_t enable_register, uintptr_t disable_register)
{
    if (irq_number > 31) {
        panic("Cannot install handler in %s with number %d\n", name, irq_number);
        return;
    }
    if (handlers[irq_number] != nullptr) {
        panic("Handler in %s with number %d already installed\n", name, irq_number);
        return;
    }

    handlers[irq_number] = handler;
    if (handler != nullptr) {
        iowrite32(enable_register, 1 << irq_number);
    } else {
        iowrite32(disable_register, 1 << irq_number);
    }
}

void interrupt_install_basic_irq_handler(uint32_t irq_number, InterruptHandler handler)
{
    install_irq("BasicIRQ", irq_number, handler, g_basic_irq_handlers, ENABLE_BASIC_IRQS, DISABLE_BASIC_IRQS);
}

void interrupt_install_irq1_handler(uint32_t irq_number, InterruptHandler handler)
{
    install_irq("IRQ1", irq_number, handler, g_irq1_handlers, ENABLE_IRQS_1, DISABLE_IRQS_1);
}

void interrupt_install_irq2_handler(uint32_t irq_number, InterruptHandler handler)
{
    install_irq("IRQ2", irq_number, handler, g_irq2_handlers, ENABLE_IRQS_2, DISABLE_IRQS_2);
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
