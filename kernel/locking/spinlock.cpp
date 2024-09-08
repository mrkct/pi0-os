#include <kernel/irq.h>
#include "spinlock.h"


void take(Spinlock& lock)
{
    while (!try_acquire(&lock.is_taken)) {
        cpu_relax();
    }
    lock.need_reenable_interrupts = irq_enabled();
    irq_disable();
}

void release(Spinlock& lock)
{
    lock.is_taken = 0;
    if (lock.need_reenable_interrupts)
        irq_enable();
}

bool is_taken(Spinlock& lock)
{
    return lock.is_taken != 0;
}
