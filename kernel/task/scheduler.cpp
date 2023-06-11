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

void scheduler_step(SuspendedTaskState*);

static constexpr uint32_t TIME_SLICE = 10000; // 10ms;

static Task* g_current_task = nullptr;
static Task* g_last_scheduled = nullptr;
static PID g_next_pid = 1;

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
    task->next_to_run = nullptr;
    task->pid = g_next_pid++;
    klib::strncpy_safe(task->name, name, sizeof(task->name));
    task->state.task_sp = static_cast<uint32_t>((areas::user_stack.end - 8) & 0xffffffff);
    task->state.spsr = 0x1f; // System mode. FIXME: Disable Fast IRQ also?

    scheduler_add(task);

    return Success;
}

static __attribute__((aligned(8))) uint8_t g_idle_task_stack[4 * _1KB];

static void idle_task()
{
    while (1) {
        api::syscall(api::SyscallIdentifiers::Yield, 0, 0, 0);
    }
}

void scheduler_init()
{
    MUST(kmalloc(sizeof(Task), g_current_task));
    g_current_task->exit_code = 0;
    g_current_task->task_state = TaskState::Running;
    g_current_task->next_to_run = nullptr;
    g_current_task->pid = 0;
    // FIXME: This assignment works only because AddressSpace only contains a pointer to ttbr0
    //        This is very risky and should be fixed.
    g_current_task->address_space = vm_current_address_space();
    klib::strncpy_safe(g_current_task->name, "idle", sizeof(g_current_task->name));
    g_last_scheduled = g_current_task;
    g_last_scheduled->state.task_sp = reinterpret_cast<uint32_t>(g_idle_task_stack + sizeof(g_idle_task_stack) - 8);
    g_last_scheduled->state.lr = reinterpret_cast<uint32_t>(idle_task);
}

static void task_free(Task* task)
{
    // TODO: Free the address space
    kfree(task);
}

Task* scheduler_current_task()
{
    return g_current_task;
}

static __attribute__((unused)) void scheduler_print()
{
    kprintf("Tasks:\n");
    kprintf("[C] %s (%p)\n", g_current_task->name, g_current_task->state.task_sp);
    for (auto* task = g_current_task->next_to_run; task != nullptr; task = task->next_to_run)
        kprintf("    %s (%p)\n", task->name, task->state.task_sp);
    kprintf("+--------------------------------------+\n");
    kprintf("| g_last_scheduled: %s (%p) \t\t|\n", g_last_scheduled->name, g_last_scheduled->state.task_sp);
    kprintf("| g_current_task:   %s (%p) \t\t|\n", g_current_task->name, g_current_task->state.task_sp);
    kprintf("+--------------------------------------+\n");
}

void scheduler_add(Task* task)
{
    task->next_to_run = nullptr;
    g_last_scheduled->next_to_run = task;
    g_last_scheduled = task;
}

void scheduler_step(SuspendedTaskState* suspended_state)
{
    auto* current = g_current_task;
    auto* next = g_current_task->next_to_run;

    if (next == nullptr)
        return;

    g_current_task->state = *suspended_state;
    *suspended_state = next->state;

    vm_switch_address_space(next->address_space);
    g_current_task = next;

    if (current->task_state != TaskState::Zombie) {
        scheduler_add(current);
    } else {
        task_free(current);
    }
}

[[noreturn]] void scheduler_begin()
{
    systimer_repeating_callback(TIME_SLICE, [](auto*) {
        // No need to do anything, triggering any IRQ will cause the scheduler to run
    });

    asm volatile(
        "mov r0, %[system_stack] \n"
        "mov r1, %[entry_point] \n"
        "cpsid if, #0x1f \n"
        "mov sp, r0 \n"
        "mov lr, #0 \n"
        "cpsie if, #0x1f \n"
        "mov pc, r1 \n"
        :
        : [system_stack] "r"(g_current_task->state.task_sp),
        [entry_point] "r"(g_current_task->state.lr)
        : "r0", "r1", "memory");

    // Silence compiler warning
    while (1)
        ;
}

}
