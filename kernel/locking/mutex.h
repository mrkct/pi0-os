#pragma once

#include <kernel/base.h>
#include "spinlock.h"


struct Mutex {
    Spinlock lock;
};

enum class MutexInitialState { Unlocked, Locked };

void mutex_init(Mutex& mutex, MutexInitialState);

void mutex_take(Mutex const& mutex);

int mutex_take_with_timeout(Mutex const& mutex, uint32_t timeout_ms);

void mutex_release(Mutex const& mutex);

bool mutex_is_locked(Mutex const& mutex);
