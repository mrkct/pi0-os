#ifndef ARM_SYSCALL_H
#define ARM_SYSCALL_H

#include <stdint.h>
#include <stddef.h>


#define ARM_SWI_SYSCALL 0x00

static inline int syscall(
    uint32_t id,
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5, uint32_t arg6
)
{
    uint32_t result;

    register uint32_t r0 asm("r0") = id;
    register uint32_t r1 asm("r1") = arg1;
    register uint32_t r2 asm("r2") = arg2;
    register uint32_t r3 asm("r3") = arg3;
    register uint32_t r4 asm("r4") = arg4;
    register uint32_t r5 asm("r5") = arg5;
	register uint32_t r6 asm("r6") = arg6;
	
	__asm__ __volatile__(
			"swi %1"
			: "=r"(r0)
			: "i"(ARM_SWI_SYSCALL),
              "r"(r0), "r"(r1), "r"(r2),
              "r"(r3), "r"(r4), "r"(r5), "r"(r6)
			: "cc", "memory");

    return result;
}


#endif
