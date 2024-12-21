#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/fcntl.h>


typedef enum SyscallIdentifiers {
    SYS_Yield = 1,
    SYS_Exit = 2,
    SYS_DebugLog = 3,
    SYS_GetPid = 4,
    SYS_Fork = 5,
    SYS_Execve = 6,
    SYS_WaitPid = 7,
    SYS_Kill = 8,

    SYS_Open = 10,
    SYS_Read = 11,
    SYS_Write = 12,
    SYS_Close = 13,
    SYS_Stat = 14,
    SYS_Seek = 15,
    SYS_CreatePipe = 16,
    SYS_Dup2 = 17,
    SYS_Select = 18,
    SYS_FStat = 19,
    SYS_MakeDirectory = 20,
    SYS_RemoveDirectory = 21,
    SYS_Link = 22,
    SYS_Unlink = 23,

    SYS_GetDateTime = 30,
    SYS_MilliSleep = 31,
} SyscallIdentifiers;



typedef enum MajorDeviceNumber {
    Maj_Reserved = 0,
    Maj_Disk = 3,
    Maj_TTY = 4,
    Maj_Console = 5,
    Maj_UART = 6,
    Maj_GPIO = 7,
    Maj_RTC = 8,
    Maj_Keyboard = 9,
    Maj_Mouse = 10,
    Maj_Input = 11,
} MajorDeviceNumber;

#include "arm/syscall.h"

#include "files.h"
// #include "process.h"
// #include "input.h"
#include "time.h"
// #include "windows.h"
