.section ".vector_table.table", "aw"
vector_table:
    ldr pc, =reset_trampoline
    ldr pc, =undefined_instruction_trampoline
    ldr pc, =software_interrupt_trampoline
    ldr pc, =prefetch_abort_trampoline
    ldr pc, =data_abort_trampoline
    nop
    ldr pc, =irq_trampoline
    ldr pc, =fast_irq_trampoline

.section ".vector_table.trampolines", "ax"
.extern irq_and_exception_handler
.macro TRAMPOLINE name, vector_offset
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
    push {r0-r12, lr}
    
    // Stack alignment
    and r1, sp, #4
    sub sp, sp, r1
    push {r1}

    mov r0, \vector_offset
    mov r1, sp
    bl irq_and_exception_handler
    
    // Restore stack alignment
    pop     {r1}
    add     sp, sp, r1

    pop     {r0-r12, lr}
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
