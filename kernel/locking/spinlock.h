#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

typedef uint32_t Spinlock;

static constexpr Spinlock SPINLOCK_START = 0;

void take(Spinlock&);

bool is_taken(Spinlock& lock);

void release(Spinlock&);

}
