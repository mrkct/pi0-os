#include <kernel/irq.h>
#include <kernel/timer.h>
#include "spinlock.h"


void spinlock_take(Spinlock const& lock)
{
    while (!try_acquire(const_cast<uint32_t*>(&lock.is_taken))) {
        cpu_relax();
    }
}

int spinlock_take_with_timeout(Spinlock const& lock, uint32_t timeout_ms)
{
    uint32_t start = get_ticks_ms();
    while (!try_acquire(const_cast<uint32_t*>(&lock.is_taken))) {
        if (get_ticks_ms() - start > timeout_ms)
            return -ERR_TIMEDOUT;
        cpu_relax();
    }
    return 0;
}

void spinlock_release(Spinlock const& lock)
{
    const_cast<uint32_t&>(lock.is_taken) = 0;
}

bool spinlock_is_taken(Spinlock const& lock)
{
    return lock.is_taken != 0;
}
