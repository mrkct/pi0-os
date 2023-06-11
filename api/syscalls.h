#pragma once

#include <stdint.h>
#include <stddef.h>

namespace api {

constexpr uint8_t SYSCALL_VECTOR = 0x10;

enum class SyscallIdentifiers : uint32_t {
    Yield = 1,
    Exit = 2,
    DebugLog = 3,
    GetProcessInfo = 4,

    OpenFile = 10,
    ReadFile = 11,
    WriteFile = 12,
    CloseFile = 13,
    Stat = 14,

    MakeDirectory = 20,
    OpenDirectory = 21,
    ReadDirectory = 22,

    GetTimeOfDay = 30,
    Sleep = 31,

    Poll = 40,
    Send = 41,

    Fork = 50,
    Exec = 51,

    GetBrk = 60,
    SetBrk = 61,
};

static inline int syscall(SyscallIdentifiers id, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    int result;
    asm volatile("mov r0, %1\n"
                 "mov r1, %2\n"
                 "mov r2, %3\n"
                 "mov r3, %4\n"
                 "svc %5\n"
                 "mov %0, r0\n"
                 : "=r"(result)
                 : "r"(static_cast<uint32_t>(id)), "r"(arg0), "r"(arg1), "r"(arg2), "i"(SYSCALL_VECTOR)
                 : "r0", "r1", "r2", "r3", "memory");
    return result;
}

typedef uint32_t PID;
struct ProcessInfo {
    PID pid;
    char name[32];
};



}
