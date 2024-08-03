.section ".text.entry", "ax"
.global _start
_start:
    b start

.section ".text", "ax"
start:
#ifndef CONFIG_ARMV6
	mrc p15, 0, r5, c0, c0, 5
	and r5, r5, #3
	cmp r5, #0
	b multicore_halt
#endif

    // Clear the BSS
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

    // NOTE: The bootloader has already setup a stack for us
    mov r0, sp
    bl kernel_main

multicore_halt:
    b multicore_halt
