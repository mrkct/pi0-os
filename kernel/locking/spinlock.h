#pragma once

#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint32_t is_taken;
    bool need_reenable_interrupts;
} Spinlock;

static constexpr Spinlock SPINLOCK_START = { 0, false };

void take(Spinlock&);

bool is_taken(Spinlock& lock);

void release(Spinlock&);
