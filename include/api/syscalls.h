#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>



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
    SYS_Select = 18,

    SYS_MakeDirectory = 20,
    SYS_ReadDirEntry = 21,

    SYS_GetDateTime = 30,
    SYS_Sleep = 31,

    SYS_Poll = 40,
    SYS_Send = 41,

    SYS_SpawnProcess = 50,
    SYS_AwaitProcess = 51,

    SYS_GetBrk = 60,
    SYS_SetBrk = 61,

    SYS_CreateWindow = 80,
    SYS_UpdateWindow = 81,
} SyscallIdentifiers;



typedef enum MajorDeviceNumber {
    Maj_TTY = 4,
    Maj_Console = 5,
    Maj_UART = 6,
    Maj_GPIO = 7,
    Maj_RTC = 8,
} MajorDeviceNumber;

#include "files.h"
#include "process.h"
#include "input.h"
#include "time.h"
#include "windows.h"
