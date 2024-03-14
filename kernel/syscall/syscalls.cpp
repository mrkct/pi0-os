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
        : m_rc((uintptr_t)(e.generic_error_code)), m_value(0)
    {
    }
    constexpr SyscallResult(uint32_t value)
        : m_rc(static_cast<uintptr_t>(GenericErrorCode::Success)), m_value(value)
    {
    }

    constexpr uintptr_t rc() const { return m_rc; }
    constexpr uintptr_t value() const { return m_value; }

private:
    uintptr_t m_rc, m_value;
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
    strncpy_safe(info.name, scheduler_current_task()->name, sizeof(info.name));

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

    size_t user_path_len = strlen(reinterpret_cast<char const*>(user_path));
    if (user_path_len > FS_MAX_PATH_LENGTH)
        return PathTooLong;

    TRY(vm_copy_from_user(scheduler_current_task()->address_space, filepath_temp_buffer, user_path, user_path_len));
    filepath_temp_buffer[user_path_len] = '\0';
    path = filepath_temp_buffer;
    return Success;
}

static SyscallResult sys$open_file(uintptr_t pathname, uint32_t flags)
{
    auto* task = scheduler_current_task();

    char const* absolute_path = nullptr;
    TRY(absolute_pathname(pathname, absolute_path));

    uint32_t fd;
    TRY(task_open_file(task, absolute_path, flags, fd));

    return { reinterpret_cast<uint32_t>(fd) };
}

static SyscallResult sys$read_file(uint32_t fd, uintptr_t user_buf, uint32_t count)
{
    auto* task = scheduler_current_task();

    FileCustody *custody;
    TRY(task_get_open_file(task, fd, custody));
    TRY(vfs_read(*custody, reinterpret_cast<uint8_t*>(user_buf), count, count));

    return count;
}

static SyscallResult sys$write_file(uint32_t fd, uintptr_t user_buf, uint32_t count)
{
    auto* task = scheduler_current_task();

    FileCustody *custody;
    TRY(task_get_open_file(task, fd, custody));
    TRY(vfs_write(*custody, reinterpret_cast<uint8_t const*>(user_buf), count, count));

    return count;
}

static SyscallResult sys$close_file(uint32_t fd)
{
    auto* task = scheduler_current_task();
    TRY(task_close_file(task, fd));

    return Success;
}

static SyscallResult sys$stat(uintptr_t pathname, uintptr_t user_stat_buf)
{
    char const* absolute_path = nullptr;
    TRY(absolute_pathname(pathname, absolute_path));

    api::Stat *stat = reinterpret_cast<api::Stat*>(user_stat_buf);
    TRY(vfs_stat(absolute_path, *stat));

    return Success;
}

static SyscallResult sys$seek_file(uint32_t fd, int32_t offset, uint32_t mode_u32)
{
    auto* task = scheduler_current_task();

    FileCustody *custody;
    TRY(task_get_open_file(task, fd, custody));
    TRY(vfs_seek(*custody, static_cast<api::FileSeekMode>(mode_u32), offset));

    return custody->seek_position;
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

static SyscallResult sys$spawn_process(uintptr_t path, uint32_t argc, const char **argv)
{
    // The args need to be copied in the new process's address space, therefore we
    // cannot pass a direct pointer to the running process's one.
    // we have to copy the entire array in kernel space, then that gets copied in
    // the new process's AS and at last we free the array in kernel space

    // TODO: This "works", but has a million memory leaks in case of failures

    char **k_argv;
    TRY(kmalloc(sizeof(char*) * argc, k_argv));
    for (size_t i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        MUST(kmalloc(sizeof(char) * len + 1, k_argv[i]));
        strncpy_safe(k_argv[i], argv[i], len + 1);
    }

    PID pid;
    TRY(task_load_user_elf_from_path(
        pid, 
        reinterpret_cast<const char*>(path), 
        (int) argc, 
        k_argv));

    for (size_t i = 0; i < argc; i++)
        kfree(k_argv[i]);
    kfree(k_argv);

    return pid;
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
        result = sys$spawn_process(static_cast<uintptr_t>(r0), r1, reinterpret_cast<const char**>(r2));
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

    r0 = result.rc();
    r1 = result.value();
}

}
