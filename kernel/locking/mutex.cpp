#include "mutex.h"

// TODO: A real implementation, this is just a placeholder

void mutex_init(Mutex& mutex, MutexInitialState state)
{
    mutex.lock = SPINLOCK_START;
    if (state == MutexInitialState::Locked)
        spinlock_take(mutex.lock);
}

void mutex_take(Mutex& mutex)
{
    spinlock_take(mutex.lock);
}

int mutex_take_with_timeout(Mutex& mutex, uint32_t timeout_ms)
{
    return spinlock_take_with_timeout(mutex.lock, timeout_ms);
}

void mutex_release(Mutex& mutex)
{
    spinlock_release(mutex.lock);
}

bool mutex_is_locked(Mutex& mutex)
{
    return spinlock_is_taken(mutex.lock);
}
