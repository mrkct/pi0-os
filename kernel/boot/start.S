.section ".text.entry", "ax"
.global _start
_start:
    mov r3, pc
    b start


.section ".text", "ax"
start:
#ifndef CONFIG_ARMV6
	mrc p15, 0, r5, c0, c0, 5
	and r5, r5, #3
	cmp r5, #0
	bne wait_for_multicore_lock
#endif
    // Zero out the BSS, note we can't do this in C because the stack also lives in the BSS
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

    ldr sp, =boot_stack_end
    bl boot_start

wait_for_multicore_lock:        // Loop while multicore_go == 0 
    ldr r7, =multicore_go
    ldr r7, [r7]
    cmp r7, #0
    beq wait_for_multicore_lock

    ldr r7, =multicore_pc       // Load and jump to address at multicore_pc
    ldr r7, [r7]
    mov pc, r7

.global activate_mmu_and_jump_to_kernel
activate_mmu_and_jump_to_kernel:
    /* 
        This function is called with these values
         r0: ttbr0 value to load 
         r1: kernel stack to load 
         r2: kernel entry point to jump to
    */

    movs r7, #0
    mcr p15, 0, r7, c7, c7, 0               // Write "Invalidate Both Caches Register"
    mcr p15, 0, r7, c8, c7, 0               // Write "Invalidate Unified TLB Register"
    mcr p15, 0, r7, c7, c10, 4              // Write "Data Memory Barrier Register"

    movs r7, #0x00000001                    // Only setup domain zero to "Client" (use permission checking)
    mcr p15, 0, r7, c3, c0, 0               // Write "Domain Access Control Register"

    mcr p15, 0, r0, c2, c0, 0               // Write "Translation Table Base Register 0"
    mcr p15, 0, r0, c2, c0, 1               // Write "Translation Table Base Register 1"

    movs r0, #0                             // All disabled, N=0
    mcr p15, 0, r0, c2, c0, 2               // "Translation Table Base Control Register"

    mrc p15, 0, r0, c1, c0, 0               // Read "System Control Register"
#ifdef CONFIG_ARMV6
    ldr r7, =#0x400000                      // Set '[U]naligned Data Access Operations' enabled
    orr r0, r0, r7                          // | Reason: https://gitlab.com/qemu-project/qemu/-/issues/1878
#endif
    orr r0, r0, #0x1                        // Set MMU Enable bit
    mcr p15, 0, r0, c1, c0, 0               // Write "System Control Register"


    mov sp, r1                              // Set the stack pointer (only for core 0)
    
    ldr r7, =multicore_pc                   // Set the program counter that the other cores will jump to
    str r2, [r7]                            
    ldr r7, =multicore_go                   // Start them up
    mov r8, #1

    mov pc, r2                              // Jump to the kernel entry point

.section ".bss", "aw"
.balign 16384
boot_stack:
    .space 8 * 1024
boot_stack_end:

.section ".data", "aw"
multicore_go: .word 0x0      // Cores != 0 will spin until this sets to something different than 0
multicore_pc: .word 0x0      // Addr that cores will jump to when they are ready to go
