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
    push {lr}                           // Note that we're pushing also LR because if an interrupt triggered
                                        // while the CPU was in Supervisor mode, then this trap would trash
                                        // that register and everything would break there once it returned
    
    // We want to also save and restore the 'User'/'System' stack pointer since they might be changed
    // by the trap handler if a context switch is needed.
    //
    // The 'sp' register is banked, and therefore not directly accessible in 'Supervisor' mode, but
    // both 'System' and 'User' modes share the same and we can change temporarily to that
    cpsid if, #0x1f                     // Change processor mode to System with disabled IRQs, FIQs and interrupts
    mov r0, sp                          // Store the current stack pointer(same one used by the user) in r0
    cpsid if, #0x13                     // Change processor mode back to Supervisor with enabled IRQs, FIQs and interrupts
    push {r0}                           // Store the current stack pointer on Supervisor mode's stack

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
    pop {r0}
    cpsid if, #0x1f                     // Change processor mode to System with disabled IRQs, FIQs and interrupts
    mov sp, r0                          // Restore the user's stack pointer
    cpsid if, #0x13                     // Change processor mode back to Supervisor with enabled IRQs, FIQs and interrupts

    pop {lr}
    pop {r0-r12}
    rfeia sp!                           // Restores the previously stored sprs and lr from the stack and
                                        // changes processor mode back to the one stored in the spsr
.endm

TRAMPOLINE reset_trampoline, #0x0

TRAMPOLINE undefined_instruction_trampoline, #0x4

TRAMPOLINE software_interrupt_trampoline, #0x8

TRAMPOLINE prefetch_abort_trampoline, #0xc

TRAMPOLINE data_abort_trampoline, #0x10

TRAMPOLINE irq_trampoline, #0x18

TRAMPOLINE fast_irq_trampoline, #0x1c


.global vector_table_data_start
vector_table_data_start:
    ldr pc, [pc, #24]
    ldr pc, [pc, #24]
    ldr pc, [pc, #24]
    ldr pc, [pc, #24]
    ldr pc, [pc, #24]
    nop
    ldr pc, [pc, #24]
    ldr pc, [pc, #24]

    reset_addr:                 .word reset_trampoline
    undefined_instruction_addr: .word undefined_instruction_trampoline
    software_interrupt_addr:    .word software_interrupt_trampoline
    prefetch_abort_addr:        .word prefetch_abort_trampoline
    data_abort_addr:            .word data_abort_trampoline
    .word 0
    irq_addr:                   .word irq_trampoline
    fast_irq_addr:              .word fast_irq_trampoline
.global vector_table_data_end
vector_table_data_end:
