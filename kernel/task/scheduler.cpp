#include <api/syscalls.h>
#include <kernel/device/systimer.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/lib/math.h>
#include <kernel/lib/string.h>
#include <kernel/locking/reentrant.h>
#include <kernel/memory/kheap.h>
#include <kernel/sizes.h>
#include <kernel/task/scheduler.h>
#include <kernel/task/elf_loader.h>

namespace kernel {

struct Queue {
    Task* head;
    Task* tail;

    constexpr Queue()
        : head(nullptr)
        , tail(nullptr)
    {
    }

    void append(Task* task)
    {
        task->next_to_run = nullptr;
        if (head == nullptr) {
            head = task;
            tail = task;
        } else {
            tail->next_to_run = task;
            tail = task;
        }
    }

    Task* pop()
    {
        if (head == nullptr)
            return nullptr;

        auto* task = head;
        head = head->next_to_run;
        if (head == nullptr)
            tail = nullptr;
        return task;
    }

    Task* remove(Task* task)
    {
        if (head == nullptr)
            return nullptr;

        if (head == task) {
            head = head->next_to_run;
            if (head == nullptr)
                tail = nullptr;
            return task;
        }

        auto* current = head;
        while (current->next_to_run != nullptr) {
            if (current->next_to_run == task) {
                current->next_to_run = current->next_to_run->next_to_run;
                if (current->next_to_run == nullptr)
                    tail = current;
                return task;
            }
            current = current->next_to_run;
        }

        return nullptr;
    }

    Task *find_by_pid(PID pid)
    {
        Task *current = head;
        while (current != tail) {
            if (current->pid == pid)
                return current;
            
            current = current->next_to_run;
        }

        return nullptr;
    }
};

static void task_free(Task* task);
void scheduler_step(SuspendedTaskState*);

constexpr size_t IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH = 1;
static PID g_next_free_pid = 0;
static Queue g_running_tasks_queue;
static Queue g_suspended_tasks_queue;

/**
 * @brief Prepares a new task by setting everything except loading the program in the address space
 * 
 * @param out_task 
 * @param name 
 * @return Error 
 */
static Error prepare_new_task(Task*& out_task, char const* name, bool is_kernel_task)
{
    Task* task;
    TRY(kmalloc(sizeof(Task), task));
    out_task = task;

    TRY(vm_create_address_space(task->address_space));

    const size_t pages_to_map_for_the_stack = klib::round_up<size_t>(areas::user_stack.end - areas::user_stack.start, _4KB) / _4KB;
    for (size_t i = 0; i < pages_to_map_for_the_stack; ++i) {
        struct PhysicalPage* task_stack_page;
        // FIXME: Rollback if this fails
        MUST(physical_page_alloc(PageOrder::_4KB, task_stack_page));
        TRY(vm_map(
            task->address_space,
            task_stack_page,
            areas::user_stack.start + i * _4KB,
            is_kernel_task ? PageAccessPermissions::PriviledgedOnly : PageAccessPermissions::UserFullAccess
        ));
    }

    task->on_task_exit_list = nullptr;
    task->exit_code = 0;
    task->task_state = TaskState::Running;

    task->state.task_sp = static_cast<uint32_t>((areas::user_stack.end - 8) & 0xffffffff);
    if (is_kernel_task) {
        // System mode
        task->state.spsr = 0x1f;
    } else {
        // User mode
        task->state.spsr = 0x10;
    }
    klib::strncpy_safe(task->name, name, sizeof(task->name));
    task->pid = g_next_free_pid++;
    task->time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH;

    task->open_files.allocated = 4;
    TRY(kmalloc(sizeof(*task->open_files.entries) * task->open_files.allocated, task->open_files.entries));
    task->open_files.len = 0;
    // POSIX software reserves 0, 1, 2 for stdin, stdout, stderr.
    // We're not POSIX, but we want to catch software that attempts to use those fds instead of
    // using the proper API.
    task->open_files.next_fd = 4;

    task->next_to_run = nullptr;

    return Success;
}

Error task_create_kernel_thread(char const* name, void (*entry)())
{
    Task *task;
    TRY(prepare_new_task(task, name, true));
    task->state.lr = reinterpret_cast<uint32_t>(entry);

    // No need to map anything as the kernel code is always mapped

    g_running_tasks_queue.append(task);

    return Success;
}

Error task_load_user_elf(const char *name, uint8_t const *elf_binary, size_t elf_binary_size)
{
    Task *task;
    TRY(prepare_new_task(task, name, false));
    
    uintptr_t entry;
    TRY(try_load_elf(elf_binary, elf_binary_size, task->address_space, entry, false));
    task->state.lr = entry;

    g_running_tasks_queue.append(task);

    return Success;
}

Error task_load_user_elf_from_path(const char *pathname)
{
    kassert(nullptr != fs_get_root());

    Stat stat;
    TRY(fs_stat(*fs_get_root(), pathname, stat));
    
    uint8_t *elf;
    TRY(kmalloc(stat.size, elf));
    
    for (size_t i = 0; i < stat.size; i++)
        elf[i] = 0;

    File file;
    
    TRY(fs_open(*fs_get_root(), pathname, file));
    size_t bytes_read;
    TRY(fs_read(file, elf, 0, stat.size, bytes_read));
    kassert(bytes_read == stat.size);
    TRY(fs_close(file));

    auto result = task_load_user_elf(pathname, elf, stat.size);
    MUST(kfree(elf));

    return result;
}

static __attribute__((aligned(8))) uint8_t g_idle_task_stack[4 * _1KB];

static void idle_task()
{
    while (1) {
        syscall(SyscallIdentifiers::SYS_Yield, 0, 0, 0, 0, 0);
    }
}

Error task_open_file(Task* task, char const* pathname, uint32_t, int& out_fd)
{
    if (fs_get_root() == nullptr)
        return DeviceNotConnected;

    File* file;
    TRY(kmalloc(sizeof(File), file));
    TRY(fs_open(*fs_get_root(), pathname, *file));

    if (task->open_files.allocated == task->open_files.len) {
        void* new_entries = task->open_files.entries;
        TRY(krealloc(new_entries, task->open_files.allocated * 2));
        task->open_files.entries = static_cast<decltype(task->open_files.entries)>(new_entries);
        task->open_files.allocated *= 2;
    }
    out_fd = task->open_files.next_fd++;
    task->open_files.entries[task->open_files.len++] = { out_fd, file };
    file_inc_ref(*file);

    return Success;
}

Error task_close_file(Task* task, int fd)
{
    for (size_t i = 0; i < task->open_files.len; i++) {
        if (task->open_files.entries[i].fd == fd) {
            file_dec_ref(*task->open_files.entries[i].file);
            kfree(task->open_files.entries[i].file);
            task->open_files.entries[i] = task->open_files.entries[--task->open_files.len];
            return Success;
        }
    }

    return BadParameters;
}

Error task_get_open_file(Task* task, int fd, File*& file)
{
    for (size_t i = 0; i < task->open_files.len; i++) {
        if (task->open_files.entries[i].fd == fd) {
            file = task->open_files.entries[i].file;
            file_inc_ref(*file);
            return Success;
        }
    }

    return NotFound;
}

Task *find_task_by_pid(PID pid)
{
    Task *t = g_running_tasks_queue.find_by_pid(pid);
    if (t != nullptr)
        return t;

    return g_suspended_tasks_queue.find_by_pid(pid);
}

Error task_add_on_exit_handler(Task *task, OnTaskExitHandler handler, void *arg)
{
    OnTaskExitHandlerListItem *item;
    TRY(kmalloc(sizeof(OnTaskExitHandlerListItem), item));
    *item = (OnTaskExitHandlerListItem){
        .callback = handler,
        .arg = arg,
        .next = task->on_task_exit_list
    };
    task->on_task_exit_list = item;

    return Success;
}

void change_task_state(Task* task, TaskState new_state)
{
    if (task->task_state == new_state)
        return;

    auto previous_state = task->task_state;
    task->task_state = new_state;
    if (g_running_tasks_queue.head == task) {
        // It's going to be moved to the correct queue by scheduler_step
        return;
    }

    switch (previous_state) {
    case TaskState::Running:
        if (!g_running_tasks_queue.remove(task))
            panic("Task %s was not found in the running queue\n", task->name);
        break;
    case TaskState::Suspended:
        if (!g_suspended_tasks_queue.remove(task))
            panic("Task %s was not found in the suspended queue\n", task->name);
        break;
    case TaskState::Zombie:
        panic("Attempting to change task %s's state, but %s is a zombie\n", task->name, task->name);
    default:
        kassert_not_reached();
    }

    switch (new_state) {
    case TaskState::Running:
        g_running_tasks_queue.append(task);
        break;
    case TaskState::Suspended:
        g_suspended_tasks_queue.append(task);
        break;
    case TaskState::Zombie: {
        
        

        task_free(task);
        break;
    }
    }
}

void scheduler_init()
{
    Task* task;
    MUST(kmalloc(sizeof(Task), task));
    *task = Task {
        .exit_code = 0,
        .task_state = TaskState::Running,

        // FIXME: This assignment works only because AddressSpace only contains a pointer to ttbr0
        //        This is very risky and should be fixed.
        .address_space = vm_current_address_space(),
        .state = {},
        .name = { 'i', 'd', 'l', 'e', '\0' },
        .pid = g_next_free_pid++,
        .time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH,

        // Should never open files the idle task
        .open_files = {
            .len = 0,
            .allocated = 0,
            .next_fd = 0,
            .entries = nullptr },
        .next_to_run = nullptr,
        .on_task_exit_list = nullptr
    };
    task->state.task_sp = reinterpret_cast<uint32_t>(g_idle_task_stack + sizeof(g_idle_task_stack) - 8);
    task->state.lr = reinterpret_cast<uint32_t>(idle_task);
    g_running_tasks_queue.append(task);
}

static void task_free(Task* task)
{
    auto *item = task->on_task_exit_list;
    while (item) {
        item->callback(item->arg);
        kfree(item);
        item = item->next;
    }

    // TODO: Free the address space
    kfree(task);
}

Task* scheduler_current_task() { return g_running_tasks_queue.head; }

void scheduler_step(SuspendedTaskState* suspended_state)
{
    auto* current = g_running_tasks_queue.head;
    kassert(current != nullptr);
    auto* next = current->next_to_run;

    if (current->task_state == TaskState::Running) {
        current->time_slice--;
        if (current->time_slice > 0)
            return;

        if (next == nullptr) {
            current->time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH;
            return;
        }
    }

    g_running_tasks_queue.pop();
    current->state = *suspended_state;
    *suspended_state = next->state;

    vm_switch_address_space(next->address_space);
    next->time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH;

    switch (current->task_state) {
    case TaskState::Running:
        g_running_tasks_queue.append(current);
        break;
    case TaskState::Suspended:
        g_suspended_tasks_queue.append(current);
        break;
    case TaskState::Zombie:
        task_free(current);
        break;
    }
}

[[noreturn]] void scheduler_begin()
{
    auto* first_task = g_running_tasks_queue.head;
    kassert(first_task != nullptr);
    kassert(klib::strcmp(first_task->name, "idle") == 0);

    asm volatile(
        "mov r0, %[system_stack] \n"
        "mov r1, %[entry_point] \n"
        "cpsid if, #0x1f \n"
        "mov sp, r0 \n"
        "mov lr, #0 \n"
        "cpsie if, #0x1f \n"
        "mov pc, r1 \n"
        :
        : [system_stack] "r"(first_task->state.task_sp),
        [entry_point] "r"(first_task->state.lr)
        : "r0", "r1", "memory");

    // Silence compiler warning
    while (1)
        ;
}

}
