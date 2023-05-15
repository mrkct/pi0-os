#include <kernel/locking/spinlock.h>

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

void spinlock_take(Spinlock& lock)
{
    while (!try_acquire(lock))
        ;
}

void spinlock_release(Spinlock& lock)
{
    lock = 0;
}

}
