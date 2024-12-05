#pragma once

#define __int64_t_defined 1
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <memory>

#include <kernel/kprintf.h>
#include <kernel/panic.h>
#include <kernel/sizes.h>
#include <kernel/error.h>
#include <kernel/memory/kheap.h>

#include <kernel/lib/more_math.h>
#include <kernel/lib/intrusivelinkedlist.h>
#include <kernel/lib/memory.h>
#include <kernel/lib/arrayutils.h>
#include <kernel/lib/more_string.h>

struct Range {
    uintptr_t start;
    uintptr_t end;

    static constexpr Range from_start_and_size(uintptr_t start, size_t size)
    {
        return { start, start + size };
    }

    constexpr bool contains(uintptr_t ptr) const
    {
        return start <= ptr && ptr < end;
    }

    constexpr size_t size() const
    {
        return end - start;
    }
};

template<typename K, typename V>
struct Pair {
    Pair(K a, V b) : first(a), second(b) {}

    union {
        K x;
        K first;
        K key;
        K left;
    };

    union {
        K y;
        K second;
        K value;
        K right;
    };
};
