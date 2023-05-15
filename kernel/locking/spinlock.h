#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

typedef uint32_t Spinlock;

#define DECLARE_SPINLOCK(name) kernel::Spinlock name = 0

consteval Spinlock spinlock_create()
{
    return 0;
}

void spinlock_take(Spinlock&);

void spinlock_release(Spinlock&);

}
