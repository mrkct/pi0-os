#include <kernel/device/systimer.h>
#include <kernel/interrupt.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/sizes.h>
#include <kernel/syscall/syscalls.h>
#include <kernel/task/scheduler.h>

namespace kernel {

static void scheduler_step(SuspendedTaskState*);

static constexpr uint32_t TIME_SLICE = 10000; // 10ms

static Task* g_current_task = nullptr;
static Task* g_last_scheduled = nullptr;
static PID g_next_pid = 1;

void yield()
{
    asm volatile("swi #1\n");
}

Error task_create_kernel_thread(Task*& out_task, char const* name, void (*entry)())
{
    static constexpr size_t KERNEL_STACK_SIZE = 4 * _1KB;

    Task* task;
    TRY(kmalloc(sizeof(Task), task));
    out_task = task;

    uint32_t* sp;
    TRY(kmalloc(KERNEL_STACK_SIZE, sp));
    sp = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(sp) + KERNEL_STACK_SIZE - 4);

    task->next_to_run = nullptr;
    task->pid = g_next_pid++;
    klib::strncpy_safe(task->name, name, sizeof(task->name));
    task->state.task_sp = reinterpret_cast<uint32_t>(sp);
    task->state.lr = reinterpret_cast<uint32_t>(entry);

    task->state.spsr = 0x1f; // System mode. FIXME: Disable Fast IRQ also?

    scheduler_add(task);

    return Success;
}

static __attribute__((aligned(8))) uint8_t g_idle_task_stack[4 * _1KB];

static void idle_task()
{
    while (1) {
        yield();
    }
}

static void _do_context_switch(SuspendedTaskState* suspended_state)
{
    // This is because if a process yielded right before it's time slice was up,
    // the timer would fire unfairly early for the next process. This way, we
    // always get a full time slice.
    systimer_repeating_callback(TIME_SLICE, _do_context_switch);
    scheduler_step(suspended_state);
}

void scheduler_init()
{
    MUST(kmalloc(sizeof(Task), g_current_task));
    g_current_task->next_to_run = nullptr;
    g_current_task->pid = 0;
    klib::strncpy_safe(g_current_task->name, "idle", sizeof(g_current_task->name));
    g_last_scheduled = g_current_task;
    g_last_scheduled->state.task_sp = reinterpret_cast<uint32_t>(g_idle_task_stack + sizeof(g_idle_task_stack) - 8);
    g_last_scheduled->state.lr = reinterpret_cast<uint32_t>(idle_task);

    interrupt_install_swi_handler(1, _do_context_switch);
}

Task* scheduler_current_task()
{
    return g_current_task;
}

static void scheduler_print()
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
    // scheduler_print();
    auto* current = g_current_task;
    auto* next = g_current_task->next_to_run;

    if (next == nullptr) {
        kprintf("No other tasks to run, carrying on\n");
        return;
    }

    g_current_task->state = *suspended_state;
    *suspended_state = next->state;

    g_current_task = next;
    scheduler_add(current);
}

[[noreturn]] void scheduler_begin()
{
    systimer_repeating_callback(TIME_SLICE, _do_context_switch);

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
