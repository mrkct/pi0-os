#pragma once

#include <stddef.h>

namespace kernel {
size_t kprintf(char const* format, ...);
}

#define __STRINGIFY(x) #x

#define panic(...)                                                       \
    do {                                                                 \
        asm volatile("cpsid i");                                         \
        kernel::kprintf("=========== KERNEL PANIC :^( ===========\n");   \
        kernel::kprintf("At %s:%d\n", __FILE__, __LINE__);               \
        kernel::kprintf(__VA_ARGS__);                                    \
        kernel::kprintf("\n========================================\n"); \
        while (1)                                                        \
            ;                                                            \
    } while (0)

#define panic_no_print(...)      \
    do {                         \
        asm volatile("cpsid i"); \
        while (1)                \
            ;                    \
    } while (0)

#define kassert_not_reached() panic("ASSERTION FAILED: not reached")

#define kassert(expr)                               \
    do {                                            \
        if (!(expr))                                \
            panic("ASSERTION FAILED: " #expr "\n"); \
    } while (0)

#define kassert_no_print(expr)                               \
    do {                                                     \
        if (!(expr))                                         \
            panic_no_print("ASSERTION FAILED: " #expr "\n"); \
    } while (0)
