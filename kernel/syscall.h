#pragma once

#include <kernel/base.h>
#include <kernel/arch/arch.h>


int dispatch_syscall(InterruptFrame *, sysarg_t syscall,
    sysarg_t arg1, sysarg_t arg2,
    sysarg_t arg3, sysarg_t arg4);
