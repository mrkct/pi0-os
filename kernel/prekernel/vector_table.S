.section ".text", "ax"
.extern irq_and_exception_handler
.macro TRAMPOLINE name, vector_offset
.global \name
\name :
/*
    HACK: The lr register normally contains the address of the next instruction.
    For most exceptions what we want is the address of the instruction that caused
    the exception, therefore we subtract 4 from lr.
    There's only one exception to this rule: Software interrupts. For these we want
    the address of the next instruction, so we don't subtract 4.
    This directive is used to disable the subtraction for software interrupts.
*/
.ifnc \vector_offset, #0x8
    sub lr, lr, #4
.endif
    srsdb sp!, #0x13                    // Stores the current mode's spsr and lr on Supervisor mode's stack
    cpsid if, #0x13                     // Change processor mode to Supervisor with disabled IRQs and FIQs
    push {r0-r12}
    
    // We want to also save and restore the user's stack pointer and leaf registers because they might be changed
    // by the IRQ handler if a context switch is needed.
    // There's no way to access it directly so we temporarily change the processor mode to System
    // (which shares the same stack as the user) and store the stack pointer in r0.
    cpsid if, #0x1f                     // Change processor mode to System with disabled IRQs, FIQs and interrupts
    mov r0, sp                          // Store the current stack pointer(same one used by the user) in r0
    mov r1, lr                          // Store the current lr(same one used by the user) in r1
    cpsid if, #0x13                     // Change processor mode back to Supervisor with enabled IRQs, FIQs and interrupts
    push {r0}                           // Store the current stack pointer on Supervisor mode's stack
    push {r1}                           // Store the current lr on Supervisor mode's stack

    // Stack alignment: The stack pointer must be 8-byte aligned before calling the handler.
    // We need to save whatever alignment correction we do here so we can undo it after the handler returns.
    and r0, sp, #4
    sub sp, sp, r0
    push {r0}

    // 2nd argument of the handler: The address at where the SavedTaskState starts
    mov r1, sp
    add r1, r1, r0                      // Stack alignment correction
    add r1, r1, #4                      // Because we pushed the alignment correction on the stack

    mov r0, \vector_offset

    bl irq_and_exception_handler
    
    // Restore stack alignment
    pop     {r1}
    add     sp, sp, r1

    // Restore the user's stack pointer
    pop {r1}
    pop {r0}
    cpsid if, #0x1f                     // Change processor mode to System with disabled IRQs, FIQs and interrupts
    mov sp, r0                          // Restore the user's stack pointer
    mov lr, r1                          // Restore the user's lr
    cpsid if, #0x13                     // Change processor mode back to Supervisor with enabled IRQs, FIQs and interrupts

    pop     {r0-r12}
    rfeia   sp!                         // Restores the previously stored sprs and lr from the stack and
                                        // changes processor mode back to the one stored in the spsr
.endm

TRAMPOLINE reset_trampoline, #0x0

TRAMPOLINE undefined_instruction_trampoline, #0x4

TRAMPOLINE software_interrupt_trampoline, #0x8

TRAMPOLINE prefetch_abort_trampoline, #0xc

TRAMPOLINE data_abort_trampoline, #0x10

TRAMPOLINE irq_trampoline, #0x18

TRAMPOLINE fast_irq_trampoline, #0x1c
