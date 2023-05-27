#include <kernel/locking/spinlock.h>
#include <kernel/task/scheduler.h>

namespace kernel {

static bool try_acquire(Spinlock& lock)
{
    uint32_t old_value = 0;
    asm volatile(
        "mov r0, #1\n"
        "swp %0, r0, [%1]\n"
        : "=r&"(old_value)
        : "r"(&lock)
        : "r0");

    return old_value == 0;
}

void take(Spinlock& lock)
{
    while (!try_acquire(lock))
        yield();
}

void release(Spinlock& lock)
{
    lock = 0;
}

bool is_taken(Spinlock& lock)
{
    return lock != 0;
}

}
