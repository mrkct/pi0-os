.section ".data.boot.early_translation_table"
.global early_translation_table
early_translation_table:
	.space 16*1024
early_translation_table_end:

.section ".data.boot"
__early_stack_start:
    .space 0x1000
__early_stack_end:

.section ".text.boot"
setup_mmu:

build_early_translation_table:
	ldr r0, =early_translation_table
	ldr r1, =early_translation_table_end

	// Clear the translation table
	mov r2, #0
1:
	str r2, [r0]
	add r0, r0, #4
	cmp r0, r1
	blo 1b

	// Then, map 32MB startin from 0x00000000 and 0xe0000000
	// to the first 32MB of physical RAM 
	ldr r0, =early_translation_table

	movs r1, #0xe0000000				// Start address of higher half kernel
	lsr r1, r1, #20						// Get the index in the translation table
	lsl r1, r1, #2						// Multiply by 4 to get the index in the translation table
	add r1, r0, r1						// Add the base address of the translation table

	movs r2, #0x2       				// Descriptor with: 1MB section
	movs r3, #0							// r3 will be the counter of the loop
1:
	str r2, [r0]
	str r2, [r1]
	add r2, r2, #0x100000				// Add 1MB
	add r0, r0, #4						// Increment the pointer in the translation table
	add r1, r1, #4						// Increment the pointer in the translation table

	add r3, r3, #1						// Increment the counter
	cmp r3, #32
	blo 1b

	// Let's also identity the peripherals at 0x20000000 (peripherals I/O, it's a 16MB hole)
	ldr r0, =early_translation_table
	movs r1, #0x20000000				// Start address of peripherals memory area
	lsr r1, r1, #20						// Get the index in the translation table
	lsl r1, r1, #2						// Multiply by 4 to get the index in the translation table
	add r1, r0, r1						// Add the base address of the translation table
	movs r0, #0x20000002				// Descriptor with: 1MB section
	movs r3, #0							// r3 will be the counter of the loop
1:
	str r0, [r1]						// Write the descriptor
	add r1, r1, #4						// Increment the pointer in the translation table
	add r0, r0, #0x100000				// Add 1MB

	add r3, r3, #1						// Increment the counter
	cmp r3, #16							// Check if we reached the end of the peripherals memory area
	blo 1b

load_and_enable_mmu:
	movs r0, #0
	mcr p15, 0, r0, c7, c7, 0			// Write "Invalidate Both Caches Register"
	mcr p15, 0, r0, c8, c7, 0			// Write "Invalidate Unified TLB Register"
	mcr p15, 0, r0, c7, c10, 4			// Write "Data Memory Barrier Register"

	movs r0, #0xffffffff				// All domains set to "Manager"(no permission checking)
	mcr p15, 0, r0, c3, c0, 0			// Write "Domain Access Control Register"

	ldr r0, =early_translation_table
	mcr p15, 0, r0, c2, c0, 0			// Write "Translation Table Base Register 0"
	mcr p15, 0, r0, c2, c0, 1			// Write "Translation Table Base Register 1"

	movs r0, #0							// All disabled, N=0
	mcr p15, 0, r0, c2, c0, 2			// "Translation Table Base Control Register"

	mrc p15, 0, r0, c1, c0, 0			// Read "System Control Register"
	orr r0, r0, #0x1					// Set MMU Enable bit
	mcr p15, 0, r0, c1, c0, 0			// Write "System Control Register"

	mov pc, lr

.globl _start
_start:
	ldr sp, =__early_stack_end
	// The RPi firmware passes some useful info in r0, r1, r2
	push {r0, r1, r2}
	bl setup_mmu

clear_bss:
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

	pop {r0, r1, r2}
	ldr r3, =kernel_main
	blx r3

halt:
	wfe
	b halt
