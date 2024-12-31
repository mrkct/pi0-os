#include <kernel/irq.h>
#include <kernel/timer.h>
#include "spinlock.h"


void spinlock_take(Spinlock& lock)
{
    while (!try_acquire(&lock.is_taken)) {
        cpu_relax();
    }
}

int spinlock_take_with_timeout(Spinlock &lock, uint32_t timeout_ms)
{
    uint32_t start = get_ticks_ms();
    while (!try_acquire(&lock.is_taken)) {
        if (get_ticks_ms() - start > timeout_ms)
            return -ERR_TIMEDOUT;
        cpu_relax();
    }
    return 0;
}

void spinlock_release(Spinlock& lock)
{
    lock.is_taken = 0;
}

bool spinlock_is_taken(Spinlock& lock)
{
    return lock.is_taken != 0;
}
