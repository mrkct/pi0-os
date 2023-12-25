#include <api/syscalls.h>
#include <kernel/datetime.h>
#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/lib/math.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/vm.h>
#include <kernel/syscall/syscalls.h>
#include <kernel/task/scheduler.h>
#include <kernel/timer.h>
#include <kernel/device/framebuffer.h>
#include <kernel/device/keyboard.h>


namespace kernel {

class SyscallResult {
public:
    constexpr SyscallResult(Error e)
        : m_rc((uintptr_t)(-static_cast<int>(e.generic_error_code)))
    {
    }
    constexpr SyscallResult(uint32_t fd)
        : m_rc(fd)
    {
    }

    constexpr uintptr_t rc() const { return m_rc; }

private:
    uintptr_t m_rc;
};

void syscall_init()
{
    interrupt_install_swi_handler(SYSCALL_VECTOR, [](auto* suspended_state) {
        dispatch_syscall(
            suspended_state->r[7],
            suspended_state->r[0],
            suspended_state->r[1],
            suspended_state->r[2],
            suspended_state->r[3],
            suspended_state->r[4]);
    });
}

static SyscallResult sys$debug_log(uintptr_t user_buf, size_t len)
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

static SyscallResult sys$exit(int error_code)
{
    auto* current = scheduler_current_task();
    current->exit_code = error_code;
    change_task_state(current, TaskState::Zombie);

    return Success;
}

static SyscallResult sys$get_process_info(uintptr_t user_buf)
{
    ProcessInfo info;
    info.pid = scheduler_current_task()->pid;
    klib::strncpy_safe(info.name, scheduler_current_task()->name, sizeof(info.name));

    TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_buf, &info, sizeof(info)));

    return Success;
}

static SyscallResult sys$get_datetime(uintptr_t user_buf)
{
    DateTime datetime;
    TRY(datetime_read(datetime));
    TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_buf, &datetime, sizeof(datetime)));

    return Success;
}

static SyscallResult sys$sleep(uint32_t ms)
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

static Error absolute_pathname(uintptr_t user_path, char const*& path)
{
    static char filepath_temp_buffer[FS_MAX_PATH_LENGTH + 1];

    size_t user_path_len = klib::strlen(reinterpret_cast<char const*>(user_path));
    if (user_path_len > FS_MAX_PATH_LENGTH)
        return PathTooLong;

    TRY(vm_copy_from_user(scheduler_current_task()->address_space, filepath_temp_buffer, user_path, user_path_len));
    filepath_temp_buffer[user_path_len] = '\0';
    path = filepath_temp_buffer;
    return Success;
}

static SyscallResult sys$open_file(uintptr_t pathname, uint32_t flags)
{
    if (fs_get_root() == nullptr)
        return NotFound;

    auto* task = scheduler_current_task();

    char const* absolute_path = nullptr;
    TRY(absolute_pathname(pathname, absolute_path));

    int fd;
    TRY(task_open_file(task, absolute_path, flags, fd));

    return fd;
}

static SyscallResult sys$read_file(int fd, uintptr_t user_buf, uint32_t count)
{
    kassert(fs_get_root() != nullptr);

    auto* task = scheduler_current_task();

    File* file;
    TRY(task_get_open_file(task, fd, file));

    size_t bytes_to_read = file->size - file->current_offset;
    if (bytes_to_read > count)
        bytes_to_read = count;

    static uint8_t buf[4096];
    size_t bytes_read = 0;
    while (bytes_read < bytes_to_read) {
        size_t bytes_to_read_now = bytes_to_read - bytes_read;
        if (bytes_to_read_now > sizeof(buf))
            bytes_to_read_now = sizeof(buf);

        size_t bytes_read_now;
        TRY(fs_read(*file, buf, file->current_offset + bytes_read, bytes_to_read_now, bytes_read_now));
        TRY(vm_copy_to_user(task->address_space, user_buf + bytes_read, buf, bytes_read_now));

        bytes_read += bytes_read_now;

        if (bytes_read_now < bytes_to_read_now)
            break;
    }
    count = bytes_read;
    file->current_offset += bytes_read;

    return count;
}

static SyscallResult sys$write_file(int fd, uintptr_t user_buf, uint32_t count)
{
    kassert(fs_get_root() != nullptr);

    (void)fd;
    (void)user_buf;
    (void)count;

    // TODO: Implement
    return NotImplemented;
}

static SyscallResult sys$close_file(int fd)
{
    kassert(fs_get_root() != nullptr);

    auto* task = scheduler_current_task();
    TRY(task_close_file(task, fd));

    return Success;
}

static SyscallResult sys$stat(uintptr_t pathname, uintptr_t user_stat_buf)
{
    if (fs_get_root() == nullptr)
        return NotFound;

    char const* absolute_path = nullptr;
    TRY(absolute_pathname(pathname, absolute_path));

    Stat stat;
    TRY(fs_stat(*fs_get_root(), absolute_path, stat));
    TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_stat_buf, &stat, sizeof(stat)));

    return Success;
}

static SyscallResult sys$seek_file(int fd, int32_t offset, uint32_t mode_u32)
{
    auto mode = static_cast<SeekMode>(mode_u32);
    if (mode != SeekMode::Current && mode != SeekMode::Start && mode != SeekMode::End)
        return BadParameters;

    kassert(fs_get_root() != nullptr);

    auto* task = scheduler_current_task();

    File* file;
    TRY(task_get_open_file(task, fd, file));
    TRY(fs_seek(*file, offset, mode));

    return file->current_offset;
}

static SyscallResult sys$blit_framebuffer(
    uintptr_t user_framebuffer,
    int32_t x, int32_t y,
    uint32_t width, uint32_t height
)
{
    // NOTE: We can convert the user's pointer to a direct one without calling vm_copy_from_user
    //       because we're sure the current address space is the same as the users
    blit_to_main_framebuffer(reinterpret_cast<uint32_t*>(user_framebuffer), x, y, width, height);

    return Success;
}

static SyscallResult sys$poll_input(uint32_t queue_id, uintptr_t user_buffer)
{
    switch (queue_id) {
    case 0: {
        KeyEvent event;
        if (!g_keyboard_events.pop(event))
            return EndOfData;
        
        TRY(vm_copy_to_user(scheduler_current_task()->address_space, user_buffer, &event, sizeof(event)));
        break;
    }
    default:
        return NotFound;
    }

    return Success;
}

static SyscallResult sys$spawn_process(uintptr_t path, uintptr_t args)
{
    // NOTE: We can convert the user's pointers to direct ones without calling vm_copy_from_user
    //       because we're sure the current address space is the same as the users

    // TODO: Support args
    (void) args;
    TRY(task_load_user_elf_from_path(reinterpret_cast<const char*>(path)));

    return Success;
}

static SyscallResult sys$await_process(int32_t pid)
{
    Task *this_task = scheduler_current_task();

    Task *task = find_task_by_pid(pid);
    if (task == nullptr)
        return NotFound;
    
    change_task_state(this_task, TaskState::Suspended);
    task_add_on_exit_handler(task, [](void *_pid) {
            Task *task = find_task_by_pid(reinterpret_cast<PID>(_pid));

            // User might have manually killed the process while it was waiting
            if (task != nullptr)
                change_task_state(task, TaskState::Running);
        },
        reinterpret_cast<void*>(scheduler_current_task()->pid)
    );

    return Success;
}

void dispatch_syscall(uint32_t& r7, uint32_t& r0, uint32_t& r1, uint32_t& r2, uint32_t& r3, uint32_t& r4)
{
    SyscallResult result = { 0 };

    (void)r3;
    (void)r4;

    switch (static_cast<SyscallIdentifiers>(r7)) {
    case SyscallIdentifiers::SYS_Yield:
        // Intentionally empty, we always yield for system calls
        break;
    case SyscallIdentifiers::SYS_Exit:
        result = sys$exit(static_cast<uintptr_t>(r0));
        break;
    case SyscallIdentifiers::SYS_DebugLog:
        result = sys$debug_log(static_cast<uintptr_t>(r0), static_cast<size_t>(r1));
        break;
    case SyscallIdentifiers::SYS_GetProcessInfo:
        result = sys$get_process_info(static_cast<uintptr_t>(r0));
        break;
    case SyscallIdentifiers::SYS_OpenFile:
        result = sys$open_file(static_cast<uintptr_t>(r0), static_cast<uint32_t>(r1));
        break;
    case SyscallIdentifiers::SYS_ReadFile:
        result = sys$read_file(static_cast<int>(r0), static_cast<uintptr_t>(r1), static_cast<size_t>(r2));
        break;
    case SyscallIdentifiers::SYS_WriteFile:
        result = sys$write_file(static_cast<int>(r0), static_cast<uintptr_t>(r1), static_cast<size_t>(r2));
        break;
    case SyscallIdentifiers::SYS_CloseFile:
        result = sys$close_file(static_cast<int>(r0));
        break;
    case SyscallIdentifiers::SYS_Seek:
        result = sys$seek_file(static_cast<int>(r0), static_cast<int32_t>(r1), static_cast<uint32_t>(r2));
        break;
    case SyscallIdentifiers::SYS_Stat:
        result = sys$stat(static_cast<uintptr_t>(r0), static_cast<uintptr_t>(r1));
        break;
    case SyscallIdentifiers::SYS_MakeDirectory:
        break;
    case SyscallIdentifiers::SYS_OpenDirectory:
        break;
    case SyscallIdentifiers::SYS_ReadDirectory:
        break;
    case SyscallIdentifiers::SYS_GetDateTime:
        result = sys$get_datetime(static_cast<uintptr_t>(r0));
        break;
    case SyscallIdentifiers::SYS_Sleep:
        result = sys$sleep(r0);
        break;
    case SyscallIdentifiers::SYS_PollInput:
        result = sys$poll_input(static_cast<uint32_t>(r0), static_cast<uintptr_t>(r1));
        break;
    case SyscallIdentifiers::SYS_SpawnProcess:
        result = sys$spawn_process(static_cast<uintptr_t>(r0), static_cast<uintptr_t>(r1));
        break;
    case SyscallIdentifiers::SYS_AwaitProcess:
        result = sys$await_process(static_cast<int32_t>(r0));
        break;
    case SyscallIdentifiers::SYS_Send:
        break;
    case SyscallIdentifiers::SYS_GetBrk:
        break;
    case SyscallIdentifiers::SYS_SetBrk:
        break;
    case SyscallIdentifiers::SYS_BlitFramebuffer:
        result = sys$blit_framebuffer(static_cast<uintptr_t>(r0), static_cast<intptr_t>(r1), static_cast<int32_t>(r2), r3, r4);
        break;
    default:
        result = InvalidSystemCall;
    }

    r7 = result.rc();
}

}
