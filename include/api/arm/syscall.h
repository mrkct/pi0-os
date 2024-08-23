#ifndef ARM_SYSCALL_H
#define ARM_SYSCALL_H

#include <stdint.h>
#include <stddef.h>


#define ARM_SWI_SYSCALL 0x10

static inline uint32_t syscall(uint32_t id, uint32_t* extra_return, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    uint32_t result, value;

    asm volatile("mov r7, %2\n"
                 "mov r0, %3\n"
                 "mov r1, %4\n"
                 "mov r2, %5\n"
                 "mov r3, %6\n"
                 "mov r4, %7\n"
                 "svc %8\n"
                 "mov %0, r0\n"
                 "mov %1, r1\n"
                 : "=r"(result), "=r"(value)
                 : "r"(id), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "i"(ARM_SWI_SYSCALL)
                 : "r0", "r1", "r2", "r3", "r4", "r7", "memory");

    if (extra_return)
        *extra_return = value;

    return result;
}


#endif
