#include <kernel/base.h>
#include <api/syscalls.h>
#include <kernel/device/systimer.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/locking/reentrant.h>
#include <kernel/task/scheduler.h>
#include <kernel/task/elf_loader.h>

// #define SCHEDULER_LOG_ENABLED 
#ifdef SCHEDULER_LOG_ENABLED
#define LOG(fmt, ...) kprintf(fmt, __VA_ARGS__)
#else
#define LOG(fmt, ...) (fmt, __VA_ARGS__)
#endif

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
        sanity_check();
        task->next_to_run = nullptr;
        if (head == nullptr) {
            head = task;
            tail = task;
        } else {
            tail->next_to_run = task;
            tail = task;
        }
        sanity_check();
    }

    Task* pop()
    {
        sanity_check();
        if (head == nullptr)
            return nullptr;

        auto* task = head;
        head = head->next_to_run;
        if (head == nullptr)
            tail = nullptr;
        sanity_check();
        return task;
    }

    Task* remove(Task* task)
    {
        sanity_check();
        if (head == nullptr) {
            sanity_check();
            return nullptr;
        }

        if (head == task) {
            head = head->next_to_run;
            if (head == nullptr)
                tail = nullptr;
            
            sanity_check();
            return task;
        }

        auto* current = head;
        while (current->next_to_run != nullptr) {
            if (current->next_to_run == task) {
                current->next_to_run = current->next_to_run->next_to_run;
                if (current->next_to_run == nullptr)
                    tail = current;
                
                sanity_check();
                return task;
            }
            current = current->next_to_run;
        }

        sanity_check();
        return nullptr;
    }

    Task *find_by_pid(api::PID pid)
    {
        Task *current = head;
        while (current != nullptr) {
            if (current->pid == pid)
                return current;
            
            current = current->next_to_run;
        }

        return nullptr;
    }

    void sanity_check()
    {
        kassert((head == nullptr && tail == nullptr) || (head != nullptr && tail != nullptr));
        if (head == nullptr)
            return;
        
        Task *last = head;
        while (last->next_to_run) {
            last = last->next_to_run;
        }
        kassert(last != nullptr);
        kassert(last == tail);
    }
};

static void task_free(Task* task);
void scheduler_step(SuspendedTaskState*);

constexpr size_t IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH = 1;
static api::PID g_next_free_pid = 0;
static Queue g_running_tasks_queue;
static Queue g_suspended_tasks_queue;
static Task *g_running_task = nullptr;

static Task *g_idle_task;
static __attribute__((aligned(8))) uint8_t g_idle_task_stack[4 * _1KB];

static void idle_task()
{
    while (1) {
        asm volatile("wfi");
    }
}

/**
 * @brief Prepares a new task by setting everything except loading the program in the address space
 * 
 * @param pid
 * @param out_task 
 * @param name 
 * @return Error 
 */
static Error prepare_new_task(
    api::PID& pid,
    Task*& out_task,
    char const* name,
    int argc,
    char const* const argv[], 
    bool is_kernel_task
)
{
    Task* task;
    TRY(kmalloc(sizeof(Task), task));
    out_task = task;

    TRY(vm_create_address_space(task->address_space));

    const size_t pages_to_map_for_the_stack = round_up<size_t>(areas::user_stack.end - areas::user_stack.start, _4KB) / _4KB;
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

    // Push argv to the process's stack
    kassert(areas::higher_half.contains(reinterpret_cast<uintptr_t>(argv)));
    vm_using_address_space(task->address_space, [&]() {
        uint8_t *sp = reinterpret_cast<uint8_t*>(task->state.task_sp);
        
        sp -= sizeof(char*) * (argc + 1);
        char **_argv = reinterpret_cast<char**>(sp);

        for (int i = 0; i < argc; i++) {
            size_t len = strlen(argv[i]) + 1;
            sp -= len;
            memcpy(sp, argv[i], len);
            _argv[i] = reinterpret_cast<char*>(sp);
        }

        sp -= 1;
        *sp = '\0';
        _argv[argc] = reinterpret_cast<char*>(sp);
        sp -= 3;
       
        // NOTE: Ensure the stack is always aligned to 8 bytes at the end!
        sp = reinterpret_cast<uint8_t*>(round_down<uint32_t>(reinterpret_cast<uint32_t>(sp), 8));
        
        sp -= sizeof(uint32_t);
        *reinterpret_cast<uint32_t*>(sp) = reinterpret_cast<uint32_t>(_argv);

        sp -= sizeof(int32_t);
        *reinterpret_cast<int32_t*>(sp) = argc;

        task->state.task_sp = reinterpret_cast<uint32_t>(sp);

        return 0;
    });

    if (is_kernel_task) {
        // System mode
        task->state.spsr = 0x1f;
    } else {
        // User mode
        task->state.spsr = 0x10;
    }
    
    strncpy(task->name, name, sizeof(task->name));
    task->name[sizeof(task->name) - 1] = '\0';
    task->pid = g_next_free_pid++;
    task->time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH;
    memset(task->open_files, 0, sizeof(task->open_files));
    task->next_to_run = nullptr;
    pid = task->pid;

    vfs_get_default_stdin_stdout_stderr(
        task->open_files[0].custody,
        task->open_files[1].custody,
        task->open_files[2].custody
    );
    return Success;
}

Error task_create_kernel_thread(
    api::PID& pid,
    char const* name,
    int argc,
    char const* const argv[],
    void (*entry)())
{
    Task *task;
    TRY(prepare_new_task(pid, task, name, argc, argv, true));
    task->state.lr = reinterpret_cast<uint32_t>(entry);

    // No need to map anything as the kernel code is always mapped

    g_running_tasks_queue.append(task);

    return Success;
}

Error task_load_user_elf(
    api::PID& pid,
    const char *name,
    int argc,
    char const* const argv[],
    uint8_t const *elf_binary,
    size_t elf_binary_size
)
{
    Task *task;
    TRY(prepare_new_task(pid, task, name, argc, argv, false));
    
    uintptr_t entry;
    TRY(try_load_elf(elf_binary, elf_binary_size, task->address_space, entry, false));
    task->state.lr = entry;

    g_running_tasks_queue.append(task);

    return Success;
}

Error task_load_user_elf_from_path(
    api::PID& pid,
    const char *pathname,
    int argc,
    char const* const argv[]
)
{
    api::Stat stat;
    TRY(vfs_stat(pathname, stat));

    uint8_t *elf;
    TRY(kmalloc(stat.size, elf));

    memset(elf, 0, stat.size);

    FileCustody file;
    MUST(vfs_open(pathname, api::OPEN_FLAG_READ, file));

    uint32_t bytes_read;
    MUST(vfs_read(file, elf, stat.size, bytes_read));
    kassert(bytes_read == stat.size);
    MUST(vfs_close(file));

    auto result = task_load_user_elf(pid, pathname, argc, argv, elf, stat.size);
    MUST(kfree(elf));

    return result;
}

int32_t task_find_free_file_descriptor(Task *task)
{
    // stdin, stdout and stderr have special handling
    for (size_t i = 3; i < array_size(task->open_files); i++) {
        if (NULL == task->open_files[i].custody.file)
            return i;
    }

    return -1;
}

static Error task_get_file_by_descriptor(Task *task, int32_t fd, TaskFileCustody *&custody)
{
    if (fd < 0 || fd >= (int32_t) array_size(task->open_files))
        return NotFound;
    
    if (task->open_files[fd].custody.file == NULL)
        return NotFound;
    
    custody = &task->open_files[fd];

    return Success;
}

Error task_get_file_by_descriptor(Task *task, int32_t fd, FileCustody *&custody)
{
    TaskFileCustody *c;
    TRY(task_get_file_by_descriptor(task, fd, c));
    custody = &c->custody;

    return Success;
}

bool task_get_custody_by_file(Task *task, File *file, TaskFileCustody* &custody)
{
    for (size_t i = 0; i < array_size(task->open_files); i++) {
        if (task->open_files[i].custody.file == file) {
            custody = &task->open_files[i];
            return true;
        }
    }

    return false;
}

Error task_reserve_n_file_descriptors(Task *task, uint32_t n, int32_t out_fds[])
{
    kassert(n == 2);

    uint32_t found = 0;
    for (uint32_t i = 0; i < array_size(task->open_files) && found < n; i++) {
        if (task->open_files[i].custody.file)
            continue;
        
        out_fds[found] = (int32_t) i;
        found++;
    }

    if (found != n)
        return TooManyOpenFiles;
    
    return Success;
}

Error task_set_file_descriptor(Task *task, int32_t fd, FileCustody custody)
{
    if (fd < 0 || fd >= (int32_t) array_size(task->open_files))
        return BadParameters;

    FileCustody *c;
    if (task_get_file_by_descriptor(task, fd, c).is_success())
        return AlreadyInUse;

    task->open_files[fd].custody = custody;
    return Success;
}

void task_inherit_file_descriptors(Task *parent, Task *child)
{
    for (uint32_t fd = 0; fd < array_size(parent->open_files); fd++) {
        if (parent->open_files[fd].custody.file == NULL)
            continue;
        
        kassert(child->open_files[fd].custody.file == NULL);
        vfs_duplicate_custody(parent->open_files[fd].custody, child->open_files[fd].custody);
    }
}

Error task_wakeup_on_fd_update(Task *task, int32_t fd)
{
    TaskFileCustody *custody;
    TRY(task_get_file_by_descriptor(task, fd, custody));
    custody->wakeup_on_update = true;

    return Success;
}

void scheduler_notify_file_update(File *file)
{
    Task *task = g_suspended_tasks_queue.head;
    while (task) {
        // We need to read this because we might remove
        // the task from its current list if it needs to be woken up
        Task *next = task->next_to_run;

        TaskFileCustody *custody;
        if (task_get_custody_by_file(task, file, custody) && custody->wakeup_on_update) {
            change_task_state(task, TaskState::Running);
        }

        task = next;
    }
}

Error task_drop_file_descriptor(Task *task, int32_t fd)
{
    if (fd < 0 || fd >= (int32_t) array_size(task->open_files) || task->open_files[fd].custody.file == NULL)
        return BadParameters;
    
    TRY(vfs_close(task->open_files[fd].custody));
    task->open_files[fd] = {};
    return Success;
}

Task *find_task_by_pid(api::PID pid)
{
    if (g_running_task->pid == pid)
        return g_running_task;

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
    LOG("[SCHED]: Moving task %s into state %s\n", task->name,
        new_state == TaskState::Running ? "RUNNING" :
        new_state == TaskState::Suspended ? "SUSPENDED": "ZOMBIE");

    if (scheduler_current_task() == task) {
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
    case TaskState::Zombie:
        task_free(task);
        break;
    }
}

void scheduler_init()
{
    MUST(kmalloc(sizeof(Task), g_idle_task));
    *g_idle_task = Task {
        .exit_code = 0,
        .task_state = TaskState::Running,

        // FIXME: This assignment works only because AddressSpace only contains a pointer to ttbr0
        //        This is very risky and should be fixed.
        .address_space = vm_current_address_space(),
        .state = {},
        .name = { 'i', 'd', 'l', 'e', '\0' },
        .pid = g_next_free_pid++,
        .time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH,
        .open_files = {},
        .next_to_run = nullptr,
        .on_task_exit_list = nullptr
    };
    g_idle_task->state.task_sp = reinterpret_cast<uint32_t>(g_idle_task_stack + sizeof(g_idle_task_stack) - 8);
    g_idle_task->state.lr = reinterpret_cast<uint32_t>(idle_task);
    g_running_task = g_idle_task;
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

Task* scheduler_current_task() { return g_running_task; }

void scheduler_step(SuspendedTaskState* suspended_state)
{
    bool old_needs_to_be_freed = false;

    auto* current = g_running_task;
    kassert(current != nullptr);
    current->state = *suspended_state;
    if (current != g_idle_task) {
        switch (current->task_state) {
        case TaskState::Running:
            g_running_tasks_queue.append(current);
            break;
        case TaskState::Suspended:
            g_suspended_tasks_queue.append(current);
            break;
        case TaskState::Zombie:
            old_needs_to_be_freed = true;
            break;
        }
    }

    auto *next = g_running_tasks_queue.pop();
    if (next == nullptr)
        next = g_idle_task;
    
    if (current == next)
        return;

    LOG("[SCHED]: Switching from %s to %s\n", current->name, next->name);
    g_running_task = next;
    *suspended_state = next->state;
    vm_switch_address_space(next->address_space);

    if (old_needs_to_be_freed)
        task_free(current);
}

[[noreturn]] void scheduler_begin()
{
    auto* first_task = g_running_task;
    kassert(first_task != nullptr);
    kassert(strcmp(first_task->name, "idle") == 0);

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
