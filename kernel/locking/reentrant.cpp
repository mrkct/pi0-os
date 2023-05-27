#include <kernel/locking/reentrant.h>
#include <kernel/panic.h>

namespace kernel {

void take(ReentrantSpinlock& lock)
{
    auto myself = scheduler_current_task()->pid;

    if (lock.count > 0 && lock.owner == myself) {
        lock.count++;
        return;
    }

    take(lock.internal_spinlock);
    lock.owner = myself;
    lock.count = 1;
}

void release(ReentrantSpinlock& lock)
{
    auto myself = scheduler_current_task()->pid;
    kassert(lock.owner == myself);
    kassert(lock.count > 0);
    lock.count--;
    if (lock.count == 0) {
        lock.owner = 0;
        release(lock.internal_spinlock);
    }
}

bool is_taken(ReentrantSpinlock& lock)
{
    return lock.count > 0;
}

}
