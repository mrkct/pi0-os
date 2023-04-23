#pragma once

#include <kernel/kprintf.h>

#define panic(...)                                                     \
    do {                                                               \
        kernel::kprintf("=========== KERNEL PANIC :^( ===========\n"); \
        kernel::kprintf(__VA_ARGS__);                                  \
        kernel::kprintf("========================================\n"); \
        while (1)                                                      \
            ;                                                          \
    } while (0)
