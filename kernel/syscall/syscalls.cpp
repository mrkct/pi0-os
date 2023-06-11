#include <api/syscalls.h>
#include <kernel/datetime.h>
#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/vm.h>
#include <kernel/syscall/syscalls.h>
#include <kernel/task/scheduler.h>

namespace kernel {

void syscall_init()
{
    interrupt_install_swi_handler(api::SYSCALL_VECTOR, [](auto* suspended_state) {
        dispatch_syscall(suspended_state->r[0], suspended_state->r[1], suspended_state->r[2], suspended_state->r[3]);
    });
}

static Error sys$debug_log(uintptr_t user_buf, size_t len)
{
    if (len > 2048)
        len = 2048;

    char* buf;
    TRY(kmalloc(len + 1, buf));
    TRY(vm_copy_from_user(scheduler_current_task()->address_space, buf, user_buf, len));
    buf[len] = '\0';
    kprintf("%s", buf);
    TRY(kfree(buf));

    return Success;
}

static Error sys$exit(int error_code)
{
    scheduler_current_task()->exit_code = error_code;
    scheduler_current_task()->task_state = TaskState::Zombie;

    return Success;
}

static Error sys$get_process_info(uintptr_t user_buf)
{
    api::ProcessInfo info;
    info.pid = scheduler_current_task()->pid;
    klib::strncpy_safe(info.name, scheduler_current_task()->name, sizeof(info.name));

    TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_buf, &info, sizeof(info)));

    return Success;
}

static Error sys$get_datetime(uintptr_t user_buf)
{
    api::DateTime datetime;
    TRY(datetime_read(datetime));
    TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_buf, &datetime, sizeof(datetime)));

    return Success;
}

void dispatch_syscall(uint32_t& r0, uint32_t& r1, uint32_t& r2, uint32_t&)
{
    Error err = Success;

    using api::SyscallIdentifiers;
    switch (static_cast<SyscallIdentifiers>(r0)) {
    case SyscallIdentifiers::Yield:
        // Intentionally empty, we always yield for system calls
        break;
    case SyscallIdentifiers::Exit:
        err = sys$exit(static_cast<uintptr_t>(r1));
        break;
    case SyscallIdentifiers::DebugLog:
        err = sys$debug_log(static_cast<uintptr_t>(r1), static_cast<size_t>(r2));
        break;
    case SyscallIdentifiers::GetProcessInfo:
        err = sys$get_process_info(static_cast<uintptr_t>(r1));
        break;
    case SyscallIdentifiers::OpenFile:
        break;
    case SyscallIdentifiers::ReadFile:
        break;
    case SyscallIdentifiers::WriteFile:
        break;
    case SyscallIdentifiers::CloseFile:
        break;
    case SyscallIdentifiers::Stat:
        break;
    case SyscallIdentifiers::MakeDirectory:
        break;
    case SyscallIdentifiers::OpenDirectory:
        break;
    case SyscallIdentifiers::ReadDirectory:
        break;
    case SyscallIdentifiers::GetDateTime:
        err = sys$get_datetime(static_cast<uintptr_t>(r1));
        break;
    case SyscallIdentifiers::Sleep:
        break;
    case SyscallIdentifiers::Poll:
        break;
    case SyscallIdentifiers::Send:
        break;
    case SyscallIdentifiers::Fork:
        break;
    case SyscallIdentifiers::Exec:
        break;
    case SyscallIdentifiers::GetBrk:
        break;
    case SyscallIdentifiers::SetBrk:
        break;
    default:
        err = InvalidSystemCall;
    }

    r0 = static_cast<uint32_t>(err.generic_error_code);
}

}
