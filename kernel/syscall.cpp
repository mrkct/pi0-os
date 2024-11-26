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
    case SYS_Yield:
        rc = sys$yield();
        break;
    case SYS_Fork:
        rc = sys$fork();
        break;
    default:
        kprintf("Unknown syscall %d\n", syscall);
        rc = -ENOSYS;
    }

    return rc;
}
