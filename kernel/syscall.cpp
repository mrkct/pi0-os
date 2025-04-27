#include <api/syscalls.h>
#include <kernel/scheduler.h>

#include "syscall.h"


int dispatch_syscall(InterruptFrame *, sysarg_t syscall,
    sysarg_t arg1, sysarg_t arg2,
    sysarg_t arg3, sysarg_t arg4)
{
    int rc;

    irq_enable();

    switch (syscall) {
    case SYS_Yield:
        rc = sys$yield();
        break;
    case SYS_Exit:
        rc = sys$exit((int) arg1);
        break;
    case SYS_GetPid:
        rc = sys$getpid();
        break;
    case SYS_Fork:
        rc = sys$fork();
        break;
    case SYS_Execve:
        rc = sys$execve((char*) arg1, (char**) arg2, (char**)arg3);
        break;
    case SYS_Poll:
        rc = sys$poll((api::PollFd*) arg1, (int) arg2, (int) arg3);
        break;
    case SYS_Open:
        rc = sys$open((char*) arg1, (int) arg2, (int) arg3);
        break;
    case SYS_Read:
        rc = sys$read((int) arg1, (char*) arg2, (size_t) arg3);
        break;
    case SYS_Write:
        rc = sys$write((int) arg1, (char*) arg2, (size_t) arg3);
        break;
    case SYS_Close:
        rc = sys$close((int) arg1);
        break;
    case SYS_Ioctl:
        rc = sys$ioctl((int) arg1, (uint32_t) arg2, (void*) arg3);
        break;
    case SYS_FStat:
        rc = sys$fstat((int) arg1, (api::Stat*) arg2);
        break;
    case SYS_Seek:
        rc = sys$seek((int) arg1, (int) arg2, (int) arg3, (uint64_t*) arg4);
        break;
    case SYS_CreatePipe:
        rc = sys$create_pipe((int*) arg1, (int*) arg2);
        break;
    case SYS_MoveFd:
        rc = sys$movefd((int) arg1, (int) arg2);
        break;
    case SYS_MMap:
        rc = sys$mmap((int) arg1, (uintptr_t) arg2, (uint32_t) arg3, (uint32_t) arg4);
        break;
    case SYS_IsTty:
        rc = sys$istty((int) arg1);
        break;
    case SYS_MilliSleep:
        rc = sys$millisleep((int) arg1);
        break;
    case SYS_Dup2:
        rc = sys$dup2((int) arg1, (int) arg2);
        break;
    case SYS_SetCwd:
        rc = sys$setcwd((char*) arg1);
        break;
    case SYS_WaitExit:
        rc = sys$waitexit((int) arg1);
        break;
    default:
        kprintf("Unknown syscall %d\n", syscall);
        rc = -ERR_NOSYS;
    }

    return rc;
}
