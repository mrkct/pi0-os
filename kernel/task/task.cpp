#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/sizes.h>
#include <kernel/task/scheduler.h>
#include <kernel/task/task.h>

namespace kernel {

static uint32_t g_next_pid = 1;

Error task_create_kernel_thread(Task*& out_task, char const* name, void (*entry)())
{
    static constexpr size_t KERNEL_STACK_SIZE = 4 * _1KB;

    Task* task;
    TRY(kmalloc(sizeof(Task), task));
    out_task = task;

    uint32_t* sp;
    TRY(kmalloc(KERNEL_STACK_SIZE, sp));
    sp = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(sp) + KERNEL_STACK_SIZE - 4);

    *(sp--) = reinterpret_cast<uint32_t>(entry);
    for (int i = 12; i > 0; i--)
        *(sp--) = 0;

    task->next_to_run = nullptr;
    task->pid = g_next_pid++;
    klib::strncpy_safe(task->name, name, sizeof(task->name));
    task->sp = reinterpret_cast<uint32_t>(sp);

    scheduler_add(task);

    return Success;
}

static __attribute__((naked)) void context_switch(uint32_t* from_sp, uint32_t* to_sp)
{
    (void)from_sp;
    (void)to_sp;

    // Note the arguments are actually used!
    // It's just that we can't pass arguments to the inline assembly
    // here or we would destroy the register state we want to save.
    // Instead, we use r0 and r1 directly

    asm volatile(
        "push {r0-r12, lr}\n"
        "str sp, [r0]\n"
        "ldr sp, [r1]\n"
        "pop {r0-r12, lr}\n"
        "cpsie i\n" // Re-enable interrupts
        "bx lr\n");
}

void context_switch_kernel_threads(Task* current, Task* to)
{
    if (current == to)
        return;

    context_switch(&current->sp, &to->sp);
}

}
