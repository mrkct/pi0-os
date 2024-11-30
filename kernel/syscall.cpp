#include <api/syscalls.h>
#include <kernel/scheduler.h>

#include "syscall.h"


int dispatch_syscall(InterruptFrame *, sysarg_t syscall,
    sysarg_t arg1, sysarg_t arg2, sysarg_t arg3,
    sysarg_t arg4, sysarg_t arg5, sysarg_t arg6)
{
    int rc;

    irq_enable();

    switch (syscall) {
    case SYS_Exit:
        rc = sys$exit((int) arg1);
        break;
    case SYS_Yield:
        rc = sys$yield();
        break;
    case SYS_Fork:
        rc = sys$fork();
        break;
    case SYS_Execve:
        rc = sys$execve((char*) arg1, (char**) arg2, (char**)arg3);
        break;
    case SYS_Open:
        rc = sys$open((char*) arg1, (int) arg2, (int) arg3);
        break;
    case SYS_Write:
        rc = sys$write((int) arg1, (char*) arg2, (size_t) arg3);
        break;
    case SYS_Read:
        rc = sys$read((int) arg1, (char*) arg2, (size_t) arg3);
        break;
    case SYS_Close:
        rc = sys$close((int) arg1);
        break;
    default:
        kprintf("Unknown syscall %d\n", syscall);
        rc = -ENOSYS;
    }

    return rc;
}
