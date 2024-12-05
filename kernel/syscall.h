#pragma once

#include <kernel/base.h>
#include <kernel/arch/arch.h>


#if __SIZEOF_POINTER__ == 4
    typedef uint32_t sysarg_t;
#elif __SIZEOF_POINTER__ == 8
    typedef uint64_t sysarg_t;
#endif

int dispatch_syscall(InterruptFrame *, sysarg_t syscall,
    sysarg_t arg1, sysarg_t arg2, sysarg_t arg3,
    sysarg_t arg4, sysarg_t arg5, sysarg_t arg6);
