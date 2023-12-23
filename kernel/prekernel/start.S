.set KERN_LINK_ADDR, 0xe0000000

.section ".text.start.boot", "ax"
.global _start
_start:
#ifdef BOARD_RASPI2
    // Shut off extra cores
	mrc p15, 0, r5, c0, c0, 5
	and r5, r5, #3
	cmp r5, #0
	bne boot_halt
#endif
    // 1. The RPi firmware passes some useful info in r0, r1, r2
    //    so let's setup the stack to save them
    ldr sp, =(_kernel_stack_end - KERN_LINK_ADDR)
    push {r0, r1, r2}

    ldr r0, =boot_msg
    bl boot_miniuart_puts

    // 2. Copy the vector table to address 0.
    //    We do this here because it's easier to access address 0
    //    before the MMU is setup
    //    NOTE: You cannot place data before 0x8000 in the binary,
    //    otherwise or the RPi firmware will refuse to load it 
    mov r0, #0
    ldr r1, =vector_table_data_start
    ldr r2, =vector_table_data_end
1:
    ldr r3, [r1]
    str r3, [r0]
    add r1, r1, #4
    add r0, r0, #4
    cmp r1, r2
    bne 1b

    // 3. Let's setup the MMU so we can start using the higher half kernel    
    
    // 3.1 First, let's zero out the translation table
    ldr r0, =(_kernel_translation_table - KERN_LINK_ADDR)
    ldr r1, =(_kernel_translation_table_end - KERN_LINK_ADDR)
    mov r2, #0
1:
    str r2, [r0]
    add r0, r0, #4
    cmp r0, r1
    blo 1b

    // 3.2 Then, let's add some entries.
    //     We'll always use "Sections", which are a quick way
    //     of mapping 1MB areas without having a 2 level page table.
    
    // 3.2.1: Start by mapping the ranges
    //            [0x00000000, 0x02000000]
    //            [0xe0000000, 0xe2000000]
    //        to the first 32MBs of physical RAM 

    ldr r0, =(_kernel_translation_table - KERN_LINK_ADDR)
    movs r1, #KERN_LINK_ADDR                // Start address of higher half kernel
    lsr r1, r1, #20                         // Get the index in the translation table
    lsl r1, r1, #2                          // Multiply by 4 to get the index in the translation table
    add r1, r0, r1                          // Add the base address of the translation table

    ldr r2, =0x602                          // "Section" to 0x0 with RW permissions for domain 0
    movs r3, #0                             // r3 will be the counter of the loop
1:
    str r2, [r0]
    str r2, [r1]
    add r2, r2, #0x100000                   // Add 1MB
    add r0, r0, #4                          // Increment the pointer in the translation table
    add r1, r1, #4                          // Increment the pointer in the translation table

    add r3, r3, #1                          // Increment the counter
    cmp r3, #32
    blo 1b

    // 3.2.2 Let's also map the peripherals so we can get some
    //       UART output for debugging.
    //       The peripherals memory area starts at 0x20000000 and its 16MBs,
    //       so we'll map it to the 16MBs after the higher half kernel
    ldr r0, =(_kernel_translation_table - KERN_LINK_ADDR)
    movs r1, #0xe2000000                    // Start address of peripherals memory area
    lsr r1, r1, #20                         // Get the index in the translation table
    lsl r1, r1, #2                          // Multiply by 4 to get the index in the translation table
    add r1, r0, r1                          // Add the base address of the translation table

    ldr r0, =0x20000602                     // "Section" to 0x2000000 with RW permissions for domain 0
    movs r3, #0                             // r3 will be the counter of the loop
1:
    str r0, [r1]                            // Write the descriptor
    add r1, r1, #4                          // Increment the pointer in the translation table
    add r0, r0, #0x100000                   // Add 1MB

    add r3, r3, #1                          // Increment the counter
    cmp r3, #16                             // Check if we reached the end of the peripherals memory area
    blo 1b

    // 4. Finally, we activate the MMU
    movs r0, #0
    mcr p15, 0, r0, c7, c7, 0               // Write "Invalidate Both Caches Register"
    mcr p15, 0, r0, c8, c7, 0               // Write "Invalidate Unified TLB Register"
    mcr p15, 0, r0, c7, c10, 4              // Write "Data Memory Barrier Register"

    movs r0, #0x00000001                    // Only setup domain zero to "Client" (use permission checking)
    mcr p15, 0, r0, c3, c0, 0               // Write "Domain Access Control Register"

    ldr r0, =(_kernel_translation_table - KERN_LINK_ADDR)
    mcr p15, 0, r0, c2, c0, 0               // Write "Translation Table Base Register 0"
    mcr p15, 0, r0, c2, c0, 1               // Write "Translation Table Base Register 1"

    movs r0, #0                             // All disabled, N=0
    mcr p15, 0, r0, c2, c0, 2               // "Translation Table Base Control Register"

    mrc p15, 0, r0, c1, c0, 0               // Read "System Control Register"
    ldr r1, =#0x400000                      // Set '[U]naligned Data Access Operations' enabled
    orr r0, r0, r1                          // | Reason: https://gitlab.com/qemu-project/qemu/-/issues/1878
    orr r0, r0, #0x1                        // Set MMU Enable bit
    mcr p15, 0, r0, c1, c0, 0               // Write "System Control Register"

    // 5. Now that the MMU is active let's jump to the higher half kernel
    ldr r0, =_stage2
    mov pc, r0

boot_halt:
    wfi
    b boot_halt

.extern reset_handler_trampoline
.extern undefined_instruction_trampoline
.extern software_interrupt_trampoline
.extern prefetch_abort_trampoline
.extern data_abort_trampoline
.extern irq_trampoline
.extern fast_irq_trampoline

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
vector_table_data_end:

.section ".text", "ax"
_stage2:
    // 6. We're now in the higher half kernel, let's change the stack address
    //    to the higher half one.
    //    Nothing changes practically, it's still the same stack
    mov r0, #KERN_LINK_ADDR
    add sp, sp, r0

    mov r0, #'D'
    bl miniuart_putc

    ldr r0, =mmu_setup
    bl miniuart_puts

    // 7. Let's setup the C runtime environment, first we need to zero out the bss
    ldr r4, =__bss_start
    ldr r9, =__bss_end
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    b 2f

1:
    stmia r4!, {r5-r8}
2:
    cmp r4, r9
    blo 1b

    // 7. Finally, let's pop out the RPi firmware values and jump to C++ land!
    pop {r0, r1, r2}
    ldr r3, =kernel_main
    blx r3

halt:
    wfe
    b halt


.section ".rodata.start", "a"
boot_msg: .ascii "OS is booting..."

.section ".rodata", "a"
mmu_setup: .ascii "MMU was setup"

.section ".data", "aw"
.global _kernel_stack_start
_kernel_stack_start:
    .space 0x8000
.global _kernel_stack_end
_kernel_stack_end:

.global _kernel_translation_table
.balign 16384
_kernel_translation_table:
    .space 16 * 1024
_kernel_translation_table_end:
.size _kernel_translation_table, _kernel_translation_table_end - _kernel_translation_table
