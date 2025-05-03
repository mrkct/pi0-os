#pragma once

#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint32_t is_taken;
} Spinlock;

static constexpr Spinlock SPINLOCK_START = { 0 };

void spinlock_take(Spinlock const&);

int spinlock_take_with_timeout(Spinlock const&, uint32_t timeout_ms);

bool spinlock_is_taken(Spinlock const& lock);

void spinlock_release(Spinlock const&);
