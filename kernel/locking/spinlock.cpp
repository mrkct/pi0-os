#include <kernel/arch/arch.h>
#include "spinlock.h"


void take(Spinlock& lock)
{
    while (!try_acquire(&lock.is_taken)) {
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
