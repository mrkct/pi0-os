#include <api/syscalls.h>
#include <kernel/datetime.h>
#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/vm.h>
#include <kernel/syscall/syscalls.h>
#include <kernel/task/scheduler.h>
#include <kernel/timer.h>

namespace kernel {

void syscall_init()
{
    interrupt_install_swi_handler(api::SYSCALL_VECTOR, [](auto* suspended_state) {
        dispatch_syscall(
            suspended_state->r[7],
            suspended_state->r[0],
            suspended_state->r[1],
            suspended_state->r[2],
            suspended_state->r[3],
            suspended_state->r[4]);
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
    auto* current = scheduler_current_task();
    current->exit_code = error_code;
    change_task_state(current, TaskState::Zombie);

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

static Error sys$sleep(uint32_t ms)
{
    auto* task = scheduler_current_task();
    kprintf("Suspending task %s\n", task->name);
    change_task_state(task, TaskState::Suspended);
    timer_exec_after(
        ms, [](void* task) {
            kprintf("Waking up task %s\n", static_cast<Task*>(task)->name);
            change_task_state(static_cast<Task*>(task), TaskState::Running);
        },
        task);

    return Success;
}

static Error prepare_pathname(uintptr_t user_path, size_t path_len, char const*& path)
{
    static char filepath_temp_buffer[FS_MAX_PATH_LENGTH + 1];

    if (path_len > FS_MAX_PATH_LENGTH)
        return PathTooLong;

    TRY(vm_copy_from_user(scheduler_current_task()->address_space, filepath_temp_buffer, user_path, path_len));
    filepath_temp_buffer[path_len] = '\0';
    path = filepath_temp_buffer;
    return Success;
}

static Error sys$open_file(uint32_t& file_descriptor, uintptr_t pathname, size_t pathname_len, uint32_t flags)
{
    if (fs_get_root() == nullptr)
        return NotFound;

    auto* task = scheduler_current_task();

    char const* absolute_path = nullptr;
    TRY(prepare_pathname(pathname, pathname_len, absolute_path));

    int fd;
    TRY(task_open_file(task, absolute_path, flags, fd));
    file_descriptor = static_cast<uint32_t>(fd);

    return Success;
}

Error sys$read_file(int fd, uintptr_t user_buf, uint32_t& count, uint64_t file_offset)
{
    kassert(fs_get_root() != nullptr);

    auto* task = scheduler_current_task();

    File* file;
    TRY(task_get_open_file(task, fd, file));

    if (file_offset > file->size)
        return BadParameters;

    size_t bytes_to_read = file->size - file_offset;
    if (bytes_to_read > count)
        bytes_to_read = count;

    static uint8_t buf[4096];
    size_t bytes_read = 0;
    while (bytes_read < count) {
        size_t bytes_to_read_now = bytes_to_read - bytes_read;
        if (bytes_to_read_now > sizeof(buf))
            bytes_to_read_now = sizeof(buf);

        size_t bytes_read_now;
        TRY(fs_read(*file, buf, file_offset + bytes_read, bytes_to_read_now, bytes_read_now));
        TRY(vm_copy_to_user(task->address_space, user_buf + bytes_read, buf, bytes_read_now));

        bytes_read += bytes_read_now;

        if (bytes_read_now < bytes_to_read_now)
            break;
    }
    count = bytes_read;

    return Success;
}

Error sys$write_file(int fd, uintptr_t user_buf, uint32_t& count, uint64_t file_offset)
{
    kassert(fs_get_root() != nullptr);

    // TODO: Implement
    return NotImplemented;
}

Error sys$close_file(int fd)
{
    kassert(fs_get_root() != nullptr);

    auto* task = scheduler_current_task();
    TRY(task_close_file(task, fd));

    return Success;
}

Error sys$stat(uintptr_t pathname, size_t pathname_len, uintptr_t user_stat_buf)
{
    if (fs_get_root() == nullptr)
        return NotFound;

    char const* absolute_path = nullptr;
    TRY(prepare_pathname(pathname, pathname_len, absolute_path));

    api::Stat stat;
    TRY(fs_stat(*fs_get_root(), absolute_path, stat));
    TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_stat_buf, &stat, sizeof(stat)));

    return Success;
}

void dispatch_syscall(uint32_t& r7, uint32_t& r0, uint32_t& r1, uint32_t& r2, uint32_t& r3, uint32_t& r4)
{
    Error err = Success;

    using api::SyscallIdentifiers;
    switch (static_cast<SyscallIdentifiers>(r7)) {
    case SyscallIdentifiers::Yield:
        // Intentionally empty, we always yield for system calls
        break;
    case SyscallIdentifiers::Exit:
        err = sys$exit(static_cast<uintptr_t>(r0));
        break;
    case SyscallIdentifiers::DebugLog:
        err = sys$debug_log(static_cast<uintptr_t>(r0), static_cast<size_t>(r1));
        break;
    case SyscallIdentifiers::GetProcessInfo:
        err = sys$get_process_info(static_cast<uintptr_t>(r0));
        break;
    /*
    case SyscallIdentifiers::OpenFile:
        err = sys$open_file(r0, static_cast<uintptr_t>(r1), static_cast<size_t>(r2), static_cast<uint32_t>(r3));
        break;
    case SyscallIdentifiers::ReadFile:
        err = sys$read_file(static_cast<int>(r0), static_cast<uintptr_t>(r1), r2, static_cast<size_t>(r3));
        break;
    case SyscallIdentifiers::WriteFile:
        err = sys$write_file(static_cast<int>(r0), static_cast<uintptr_t>(r1), r2, static_cast<size_t>(r3));
        break;
    case SyscallIdentifiers::CloseFile:
        err = sys$close_file(static_cast<int>(r0));
        break;
    */
    case SyscallIdentifiers::Stat:
        err = sys$stat(static_cast<uintptr_t>(r0), static_cast<size_t>(r1), static_cast<uintptr_t>(r2));
        break;
    case SyscallIdentifiers::MakeDirectory:
        break;
    case SyscallIdentifiers::OpenDirectory:
        break;
    case SyscallIdentifiers::ReadDirectory:
        break;
    case SyscallIdentifiers::GetDateTime:
        err = sys$get_datetime(static_cast<uintptr_t>(r0));
        break;
    case SyscallIdentifiers::Sleep:
        err = sys$sleep(r0);
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

    r7 = static_cast<uint32_t>(err.generic_error_code);
}

}
