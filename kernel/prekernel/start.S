.align 8
.section ".data.boot"
__early_stack_start:
    .space 0x8000
__early_stack_end:

.section ".text.boot"
.globl _start
_start:
	ldr sp, =__early_stack_end

clear_bss:
/*
    Note that r0, r1 and r2 contain useful information we want to pass
    to kernel_main, so let's not use them to preserve them.
*/
	ldr r4, =__bss_start
	ldr r9, =__bss_end
	mov r5, #0
	mov r6, #0
	mov r7, #0
	mov r8, #0
	b       2f
 
1:
	stmia r4!, {r5-r8}
2:
	cmp r4, r9
	blo 1b

	ldr r3, =kernel_main
	blx r3

halt:
	wfe
	b halt

