#pragma once

#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint32_t is_taken;
} Spinlock;

static constexpr Spinlock SPINLOCK_START = { 0 };

void spinlock_take(Spinlock&);

int spinlock_take_with_timeout(Spinlock&, uint32_t timeout_ms);

bool spinlock_is_taken(Spinlock& lock);

void spinlock_release(Spinlock&);
