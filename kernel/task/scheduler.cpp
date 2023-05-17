#include <kernel/interrupt.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/task/scheduler.h>

namespace kernel {

static Task* g_current_task = nullptr;
static Task* g_last_scheduled = nullptr;

void scheduler_init()
{
    MUST(kmalloc(sizeof(Task), g_current_task));
    g_current_task->next_to_run = nullptr;
    g_current_task->pid = 0;
    g_current_task->sp = 0;
    klib::strncpy_safe(g_current_task->name, "kernel", sizeof(g_current_task->name));
    g_last_scheduled = g_current_task;
}

static void scheduler_print()
{
    kprintf("Tasks:\n");
    kprintf("[C] %s (%p)\n", g_current_task->name, g_current_task->sp);
    for (auto* task = g_current_task->next_to_run; task != nullptr; task = task->next_to_run)
        kprintf("    %s (%p)\n", task->name, task->sp);
    kprintf("+--------------------------------------+\n");
    kprintf("| g_last_scheduled: %s (%p) \t\t|\n", g_last_scheduled->name, g_last_scheduled->sp);
    kprintf("| g_current_task:   %s (%p) \t\t|\n", g_current_task->name, g_current_task->sp);
    kprintf("+--------------------------------------+\n");
}

void scheduler_add(Task* task)
{
    task->next_to_run = nullptr;
    g_last_scheduled->next_to_run = task;
    g_last_scheduled = task;
}

void scheduler_step()
{
    // scheduler_print();
    auto* current = g_current_task;
    auto* next = g_current_task->next_to_run;

    if (next == nullptr) {
        kprintf("No other tasks to run, carrying on\n");
        return;
    }

    g_current_task = next;
    scheduler_add(current);

    // kprintf("Switching from %s (%p) to %s (%p)\n", current->name, current->sp, next->name, next->sp);
    context_switch_kernel_threads(current, next);
}

}
