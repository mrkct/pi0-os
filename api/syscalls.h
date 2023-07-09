#pragma once

#include <stddef.h>
#include <stdint.h>

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

    GetDateTime = 30,
    Sleep = 31,

    Poll = 40,
    Send = 41,

    Fork = 50,
    Exec = 51,

    GetBrk = 60,
    SetBrk = 61,
};

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
                 : "r"(static_cast<uint32_t>(id)), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "i"(SYSCALL_VECTOR)
                 : "r0", "r1", "r2", "r3", "r4", "r7", "memory");
    return result;
}

typedef uint32_t PID;
struct ProcessInfo {
    PID pid;
    char name[32];
};

struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    uint64_t ticks_since_boot;
};

struct Stat {
    bool is_directory;
    uint64_t size;
};

}
