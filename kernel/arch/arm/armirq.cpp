#include <kernel/syscall.h>
#include <kernel/memory/vm.h>
#include <kernel/scheduler.h>

#include "armirq.h"
#include "armv6mmu.h"


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

#define FORMAT_TASK_STATE                               \
    "\t r0: %x\t r1: %x\t r2: %x\t r3: %x\n"            \
    "\t r4: %x\t r5: %x\t r6: %x\t r7: %x\n"            \
    "\t r8: %x\t r9: %x\t r10: %x\t r11: %x\n"          \
    "\t spsr: %x"

#define FORMAT_ARGS_TASK_STATE(state)                   \
    (state)->r[0],  (state)->r[1],                      \
    (state)->r[2],  (state)->r[3],                      \
    (state)->r[4],  (state)->r[5],                      \
    (state)->r[6],  (state)->r[7],                      \
    (state)->r[8],  (state)->r[9],                      \
    (state)->r[10], (state)->r[11],                     \
    (state)->spsr


extern "C" uint8_t vector_table_data_start[];
extern "C" uint8_t vector_table_data_end[];

static void data_abort_handler(InterruptFrame* state);
static void prefetch_abort_handler(InterruptFrame* suspended_state);
static void undefined_instruction_handler(InterruptFrame *suspended_state);



extern "C" void irq_and_exception_handler(uint32_t vector_offset, InterruptFrame* frame)
{
    auto vector_index = vector_offset / 4;
    if (vector_index > 8)
        panic("UNEXPECTED VECTOR OFFSET: %x\n", vector_offset);

    switch (static_cast<InterruptVector>(vector_index)) {
    case InterruptVector::SoftwareInterrupt: {
        auto swi_number = *reinterpret_cast<uint32_t*>(frame->lr - 4) & 0xff;
        if (swi_number == ARM_SWI_SYSCALL) {
            frame->r[0] = dispatch_syscall(frame, frame->r[0],
                frame->r[1], frame->r[2], frame->r[3],
                frame->r[4]);
        }
        break;
    }
    case InterruptVector::IRQ: {
        dispatch_irq(frame);
        break;
    }
    
    case InterruptVector::PrefetchAbort:
        prefetch_abort_handler(frame);
        break;
    
    case InterruptVector::DataAbort:
        data_abort_handler(frame);
        break;
    
    case InterruptVector::UndefinedInstruction:
        undefined_instruction_handler(frame);
        break;
    
    case InterruptVector::FIQ:
    default:
        panic("Unexpected FastIRQ! They're not supported yet!\n");
    }
}

static void data_abort_handler(InterruptFrame* state)
{
    state->lr -= 8;

    uintptr_t faulting_addr = read_fault_address_register();
    auto result = vm_try_fix_page_fault(faulting_addr);
    if (result == PageFaultHandlerResult::Fixed) {
        kprintf("[NOTE]: Data abort while accessing %p, but fixed :)\n", faulting_addr);
        return;
    }
    
    if (result == PageFaultHandlerResult::ProcessFatal) {
        uint32_t dfsr = read_dfsr();
        uint32_t fault_status = dfsr_fault_status(dfsr);
        
        kprintf(
            "[DATA ABORT]: Process %s crashed\n"
            "Reason: %s accessing memory address %p while executing instruction %p\n"
            FORMAT_TASK_STATE,
            cpu_current_process()->name,
            dfsr_status_to_string(fault_status),
            faulting_addr,
            state->lr,
            FORMAT_ARGS_TASK_STATE(state)
        );
        // TODO: change_task_state(scheduler_current_task(), TaskState::Zombie);
        // TODO: scheduler_step(state);
        todo();
        return;
    }

    kassert(result == PageFaultHandlerResult::KernelFatal);
    
    uint32_t dfsr = read_dfsr();
    uint32_t fault_status = dfsr_fault_status(dfsr);
    panic(
        "[DATA ABORT]: %s accessing memory address %p while executing instruction %p\n"
        FORMAT_TASK_STATE,
        dfsr_status_to_string(fault_status),
        faulting_addr,
        state->lr,
        FORMAT_ARGS_TASK_STATE(state));
    /*
    FIXME: Uncomment this once the scheduler is implemented
    panic(
        "[DATA ABORT]\n"
        "Running process %s\n"
        "Reason: %s accessing memory address %p while executing instruction %p\n"
        FORMAT_TASK_STATE,
        get_running_task_name(),
        dfsr_status_to_string(fault_status),
        faulting_addr,
        state->lr,
        FORMAT_ARGS_TASK_STATE(state)
    );
    */
}

static void prefetch_abort_handler(InterruptFrame*)
{
    panic("unhandled PREFETCH_ABORT");
}

static void undefined_instruction_handler(InterruptFrame*)
{
    panic("unhandled UNDEFINED_INSTRUCTION");
}

void arch_irq_init()
{
    PhysicalPage *page;
    MUST(physical_page_alloc(PageOrder::_4KB, page));

    void *vector_table = (void*) phys2virt(page2addr(page));
    memcpy(vector_table, vector_table_data_start, vector_table_data_end - vector_table_data_start);

    MUST(vm_map(vm_current_address_space(), page, 0, PageAccessPermissions::PriviledgedOnly));
}

extern "C" void _arch_context_switch(ContextSwitchFrame **from, ContextSwitchFrame *to);

void arch_context_switch(ContextSwitchFrame **from, ContextSwitchFrame *to)
{
    kassert(is_supervisor_mode());
    _arch_context_switch(from, to);
}

extern "C" void pop_iframe_and_return();

void arch_create_initial_kernel_stack(
    void **kernel_stack_ptr,
    InterruptFrame **out_iframe,
    uintptr_t userstack,
    uintptr_t entrypoint,
    bool privileged
)
{
    auto *sp = reinterpret_cast<uint8_t*>(*kernel_stack_ptr);
    sp -= sizeof(InterruptFrame);
    auto *iframe = reinterpret_cast<InterruptFrame*>(sp);
    for (uint32_t i = 0; i < 13; i++)
        iframe->r[i] = i;
    iframe->user_lr = 0;
    iframe->user_sp = userstack;
    iframe->lr = entrypoint;
    iframe->spsr = (privileged ? 0x1f : 0x10);
    *out_iframe = iframe;

    sp -= sizeof(ContextSwitchFrame);
    auto *ctx = reinterpret_cast<ContextSwitchFrame*>(sp);
    // just because it makes debugging easier
    for (uint32_t i = 0; i < 13; i++)
        ctx->r[i] = i;
    ctx->lr = reinterpret_cast<uintptr_t>(pop_iframe_and_return);

    *kernel_stack_ptr = sp;
}
