#pragma once

#include <kernel/locking/spinlock.h>
#include <kernel/task/scheduler.h>

namespace kernel {

struct ReentrantSpinlock {
    Spinlock internal_spinlock;
    api::PID owner;
    size_t count;
};

static constexpr ReentrantSpinlock REENTRANT_SPINLOCK_START = { SPINLOCK_START, 0, 0 };

void take(ReentrantSpinlock& lock);

void release(ReentrantSpinlock& lock);

bool is_taken(ReentrantSpinlock& lock);

}
