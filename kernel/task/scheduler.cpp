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
};

static void task_free(Task* task);
void scheduler_step(SuspendedTaskState*);

constexpr size_t IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH = 1;
static api::PID g_next_free_pid = 0;
static Queue g_running_tasks_queue;
static Queue g_suspended_tasks_queue;

Error task_create_kernel_thread(Task*& out_task, char const* name, void (*entry)())
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
        TRY(vm_map(task->address_space, task_stack_page, areas::user_stack.start + i * _4KB));
    }

    // TODO: Ask the loader to map the executable into the address space and return the entry point
    task->state.lr = reinterpret_cast<uint32_t>(entry);

    task->exit_code = 0;
    task->task_state = TaskState::Running;

    task->state.task_sp = static_cast<uint32_t>((areas::user_stack.end - 8) & 0xffffffff);
    task->state.spsr = 0x1f; // System mode. FIXME: Disable Fast IRQ also?

    klib::strncpy_safe(task->name, name, sizeof(task->name));
    task->pid = g_next_free_pid++;
    task->time_slice = IRQS_PER_TASK_BEFORE_CONTEXT_SWITCH;
    task->next_to_run = nullptr;

    g_running_tasks_queue.append(task);

    return Success;
}

static __attribute__((aligned(8))) uint8_t g_idle_task_stack[4 * _1KB];

static void idle_task()
{
    while (1) {
        api::syscall(api::SyscallIdentifiers::Yield, 0, 0, 0);
    }
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
    case TaskState::Zombie:
        task_free(task);
        break;
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
        .next_to_run = nullptr,
    };
    task->state.task_sp = reinterpret_cast<uint32_t>(g_idle_task_stack + sizeof(g_idle_task_stack) - 8);
    task->state.lr = reinterpret_cast<uint32_t>(idle_task);
    g_running_tasks_queue.append(task);
}

static void task_free(Task* task)
{
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
