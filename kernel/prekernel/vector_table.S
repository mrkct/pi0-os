// Define section "vector_table.boot" in assembly
.section ".vector_table.table", "a"
vector_table:
    ldr pc, =reset_handler_trampoline
    ldr pc, =undefined_instruction_handler
    ldr pc, =software_interrupt_handler
    ldr pc, =prefetch_abort_handler
    ldr pc, =data_abort_handler
    nop
    ldr pc, =irq_handler
    ldr pc, =fiq_handler

.section ".text.vector_trampolines", "ax"
.align 2
reset_handler_trampoline:
    1: b 1b

undefined_instruction_handler:
    1: b 1b

.extern software_interrupt_handler_c
software_interrupt_handler:
    push {r0-r7}
    push {lr}

    // This aligns the stack to 8 bytes in case it was not aligned
    and r1, sp, #4
    sub sp, sp, r1
    push {r1}

    mov r0, sp
    bl software_interrupt_handler_c
    
    // Restore stack alignment
    pop {r1}
    add sp, sp, r1

    pop {lr}
    pop {r0-r7}
    bx lr

prefetch_abort_handler:
    1: b 1b

data_abort_handler:
    1: b 1b

irq_handler:
    1: b 1b

fiq_handler:
    1: b 1b
