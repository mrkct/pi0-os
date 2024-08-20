#include <kernel/interrupt.h>
#include "spinlock.h"


static bool try_acquire(Spinlock& lock)
{
    uint32_t old_value = 0;
    asm volatile(
        "mov r0, #1\n"
        "swp %0, r0, [%1]\n"
        : "=r&"(old_value)
        : "r"(&lock.is_taken)
        : "r0");

    return old_value == 0;
}

void take(Spinlock& lock)
{
    while (!try_acquire(lock)) {
        asm volatile("wfe");
    }
    lock.need_reenable_interrupts = interrupt_are_enabled();
    interrupt_disable();
}

void release(Spinlock& lock)
{
    lock.is_taken = 0;
    if (lock.need_reenable_interrupts)
        interrupt_enable();
}

bool is_taken(Spinlock& lock)
{
    return lock.is_taken != 0;
}
