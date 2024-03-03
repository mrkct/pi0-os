#include <kernel/device/io.h>
#include <kernel/error.h>
#include <kernel/panic.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/lib/memory.h>
#include <kernel/task/scheduler.h>


namespace kernel {

static inline const char *get_running_task_name()
{
    return scheduler_current_task() ? scheduler_current_task()->name : "kernel";
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

static InterruptHandler g_swi_handlers[256] = { nullptr };
static InterruptHandler g_basic_irq_handlers[32] = { nullptr };
static InterruptHandler g_irq1_handlers[32] = { nullptr };
static InterruptHandler g_irq2_handlers[32] = { nullptr };


static void software_interrupt_handler(SuspendedTaskState* suspended_state)
{
    auto swi_number = *reinterpret_cast<uint32_t*>(suspended_state->lr - 4) & 0xff;

    if (g_swi_handlers[swi_number] == nullptr) {
        kprintf("Unknown software interrupt: %d\n", swi_number);
        return;
    }

    g_swi_handlers[swi_number](suspended_state);
    scheduler_step(suspended_state);
}

static void data_abort_handler(SuspendedTaskState* state)
{
    state->task_lr -= 8;

    uintptr_t faulting_addr = read_fault_address_register();
    auto result = vm_try_fix_page_fault(faulting_addr);
    if (result == PageFaultHandlerResult::Fixed)
        return;
    
    if (result == PageFaultHandlerResult::ProcessFatal) {
        uint32_t dfsr = read_dfsr();
        uint32_t fault_status = dfsr_fault_status(dfsr);
        kprintf(
            "[DATA ABORT]: Process %s crashed\n"
            "Reason: %s accessing memory address %p while executing instruction %p\n"
            FORMAT_TASK_STATE,
            get_running_task_name(),
            dfsr_status_to_string(fault_status),
            faulting_addr,
            state->task_lr,
            FORMAT_ARGS_TASK_STATE(state)
        );
        change_task_state(scheduler_current_task(), TaskState::Zombie);
        scheduler_step(state);
        return;
    }

    kassert(result == PageFaultHandlerResult::KernelFatal);
    uint32_t dfsr = read_dfsr();
    uint32_t fault_status = dfsr_fault_status(dfsr);
    panic(
        "[DATA ABORT]\n"
        "Running process %s\n"
        "Reason: %s accessing memory address %p while executing instruction %p\n"
        FORMAT_TASK_STATE,
        get_running_task_name(),
        dfsr_status_to_string(fault_status),
        faulting_addr,
        state->task_lr,
        FORMAT_ARGS_TASK_STATE(state)
    );
}

static void prefetch_abort_handler(SuspendedTaskState* suspended_state)
{
    panic("unhandled PREFETCH_ABORT");
}

static void undefined_instruction_handler(SuspendedTaskState *suspended_state)
{
    panic("unhandled UNDEFINED_INSTRUCTION");
}


static void irq_handler(SuspendedTaskState* suspended_state)
{
    kassert_no_print(!interrupt_are_enabled());
    uint32_t basic, pending1, pending2;
    basic = ioread32<uint32_t>(IRQ_BASIC_PENDING);
    pending1 = ioread32<uint32_t>(IRQ_PENDING_1);
    pending2 = ioread32<uint32_t>(IRQ_PENDING_2);

    static constexpr uint32_t ONE_OR_MORE_BITS_SET_IN_PENDING1 = 1 << 8;
    static constexpr uint32_t ONE_OR_MORE_BITS_SET_IN_PENDING2 = 1 << 9;

    bool also_check_pending1 = basic & ONE_OR_MORE_BITS_SET_IN_PENDING1;
    bool also_check_pending2 = basic & ONE_OR_MORE_BITS_SET_IN_PENDING2;

    // We only care about the first 10 bits because the rest are repeated
    // IRQs also readable from the "pending" registers
    for (int i = 0; i < 10; ++i) {
        auto mask = 1 << i;
        if (mask == ONE_OR_MORE_BITS_SET_IN_PENDING1 || mask == ONE_OR_MORE_BITS_SET_IN_PENDING2)
            continue;

        if (basic & mask) {
            if (g_basic_irq_handlers[i] == nullptr)
                panic("Unhandled IRQ %d\n", i);

            g_basic_irq_handlers[i](suspended_state);
        }
    }

    if (also_check_pending1) {
        for (int i = 0; i < 32; ++i) {
            auto mask = 1 << i;

            if (pending1 & mask) {
                if (g_irq1_handlers[i] == nullptr)
                    panic("Unhandled IRQ1 %d\n", i);

                g_irq1_handlers[i](suspended_state);
            }
        }
    }

    if (also_check_pending2) {
        for (int i = 0; i < 32; ++i) {
            auto mask = 1 << i;

            if (pending2 & mask) {
                if (g_irq2_handlers[i] == nullptr)
                    panic("Unhandled IRQ2 %d\n", i);

                g_irq2_handlers[i](suspended_state);
            }
        }
    }

    scheduler_step(suspended_state);
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
extern "C" void irq_and_exception_handler(uint32_t vector_offset, SuspendedTaskState* suspended_state)
{
    auto vector_index = vector_offset / 4;
    if (vector_index > 8)
        panic("UNEXPECTED VECTOR OFFSET: %x\n", vector_offset);

    switch (static_cast<InterruptVector>(vector_index)) {
    case InterruptVector::SoftwareInterrupt:
        software_interrupt_handler(suspended_state);
        break;

    case InterruptVector::IRQ:
        irq_handler(suspended_state);
        break;
    
    case InterruptVector::PrefetchAbort:
        prefetch_abort_handler(suspended_state);
        break;
    
    case InterruptVector::DataAbort:
        data_abort_handler(suspended_state);
        break;
    
    case InterruptVector::UndefinedInstruction:
        undefined_instruction_handler(suspended_state);
        break;
    
    case InterruptVector::FIQ:
    default:
        panic(
            "Unhandled vector %d caused by instruction %p "
            "while running task %s\n" FORMAT_TASK_STATE,
            vector_index, suspended_state->task_lr,
            get_running_task_name(),
            FORMAT_ARGS_TASK_STATE(suspended_state)
        );
    }
}

}
