#pragma once

#include <stddef.h>

namespace kernel {
size_t kprintf(char const* format, ...);
}

#define panic(...)                                                     \
    do {                                                               \
        kernel::kprintf("=========== KERNEL PANIC :^( ===========\n"); \
        kernel::kprintf(__VA_ARGS__);                                  \
        kernel::kprintf("\n========================================\n"); \
        while (1)                                                      \
            ;                                                          \
    } while (0)

#define kassert_not_reached() panic("ASSERTION FAILED: not reached\n")

#define kassert(expr)                               \
    do {                                            \
        if (!(expr))                                \
            panic("ASSERTION FAILED: " #expr "\n"); \
    } while (0)
