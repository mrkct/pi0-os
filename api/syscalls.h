#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


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

    SYS_MakeDirectory = 20,
    SYS_OpenDirectory = 21,
    SYS_ReadDirectory = 22,

    SYS_GetDateTime = 30,
    SYS_Sleep = 31,

    SYS_Poll = 40,
    SYS_Send = 41,

    SYS_Fork = 50,
    SYS_Exec = 51,

    SYS_GetBrk = 60,
    SYS_SetBrk = 61,
} SyscallIdentifiers;

typedef enum OpenFileModes {
    MODE_READ   = 1 << 0,
    MODE_WRITE  = 1 << 1,
    MODE_APPEND = 2 << 2
} OpenFileModes;

typedef enum SeekModes {
    Start, Current, End
} SeekModes;

static inline int syscall(SyscallIdentifiers id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    int result;
    asm volatile("mov r7, %1\n"
                 "mov r0, %2\n"
                 "mov r1, %3\n"
                 "mov r2, %4\n"
                 "mov r3, %5\n"
                 "mov r4, %6\n"
                 "svc %7\n"
                 "mov %0, r7\n"
                 : "=r"(result)
                 : "r"((uint32_t) (id)), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "i"(SYSCALL_VECTOR)
                 : "r0", "r1", "r2", "r3", "r4", "r7", "memory");
    return result;
}

typedef uint32_t PID;
typedef struct ProcessInfo {
    PID pid;
    char name[32];
} ProcessInfo;

typedef struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    uint64_t ticks_since_boot;
} DateTime;

typedef struct Stat {
    bool is_directory;
    uint64_t size;
} Stat;
