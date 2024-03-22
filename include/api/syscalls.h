#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define SYSCALL_VECTOR 0x10

typedef enum SyscallIdentifiers {
    SYS_Yield = 1,
    SYS_Exit = 2,
    SYS_DebugLog = 3,
    SYS_GetProcessInfo = 4,

    SYS_OpenFile = 10,
    SYS_ReadFile = 11,
    SYS_WriteFile = 12,
    SYS_CloseFile = 13,
    SYS_Stat = 14,
    SYS_Seek = 15,
    SYS_CreatePipe = 16,
    SYS_Dup2 = 17,

    SYS_MakeDirectory = 20,
    SYS_OpenDirectory = 21,
    SYS_ReadDirectory = 22,

    SYS_GetDateTime = 30,
    SYS_Sleep = 31,

    SYS_Poll = 40,
    SYS_Send = 41,

    SYS_SpawnProcess = 50,
    SYS_AwaitProcess = 51,

    SYS_GetBrk = 60,
    SYS_SetBrk = 61,

    SYS_PollInput = 70,

    SYS_BlitFramebuffer = 80,
} SyscallIdentifiers;


static inline uint32_t syscall(SyscallIdentifiers id, uint32_t* extra_return, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
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
                 : "r"((uint32_t)(id)), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "i"(SYSCALL_VECTOR)
                 : "r0", "r1", "r2", "r3", "r4", "r7", "memory");

    if (extra_return)
        *extra_return = value;

    return result;
}

#include "files.h"
#include "process.h"
#include "input.h"
#include "time.h"
