#include <kernel/syscall.h>
#include <kernel/memory/vm.h>

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
    "\t sp: %p\n"                                       \
    "\t user lr: %p\n"                                  \
    "\t spsr: %x"

#define FORMAT_ARGS_TASK_STATE(state)                   \
    (state)->r[0],  (state)->r[1],                      \
    (state)->r[2],  (state)->r[3],                      \
    (state)->r[4],  (state)->r[5],                      \
    (state)->r[6],  (state)->r[7],                      \
    (state)->r[8],  (state)->r[9],                      \
    (state)->r[10], (state)->r[11],                     \
    (state)->task_sp,                                   \
    (state)->task_lr,                                   \
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
            dispatch_syscall(frame, frame->r[0]);
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
        /*
        FIXME: Uncomment this when the kernel has a proper crash handler

        uint32_t dfsr = read_dfsr();
        uint32_t fault_status = dfsr_fault_status(dfsr);
        
        kprintf(
            "[DATA ABORT]: Process %s crashed\n"
            "Reason: %s accessing memory address %p while executing instruction %p\n"
            FORMAT_TASK_STATE,
            get_running_task_name(),
            dfsr_status_to_string(fault_status),
            faulting_addr,
            state->lr,
            FORMAT_ARGS_TASK_STATE(state)
        );
        change_task_state(scheduler_current_task(), TaskState::Zombie);
        scheduler_step(state);
        */
        todo();
        return;
    }

    kassert(result == PageFaultHandlerResult::KernelFatal);
    
    uint32_t dfsr = read_dfsr();
    uint32_t fault_status = dfsr_fault_status(dfsr);
    panic(
        "[DATA ABORT]: %s accessing memory address %p while executing instruction %p\n",
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

static void prefetch_abort_handler(InterruptFrame* suspended_state)
{
    panic("unhandled PREFETCH_ABORT");
}

static void undefined_instruction_handler(InterruptFrame *suspended_state)
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
