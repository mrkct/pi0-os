#pragma once

#include <stddef.h>

static inline void irq_disable();
#include <kernel/irq.h>

size_t kprintf(char const* format, ...);

#ifndef UNIT_TEST

#define panic(...)                                                       \
    do {                                                                 \
        irq_disable();                                                   \
        kprintf("=========== KERNEL PANIC :^( ===========\n");           \
        kprintf("At %s:%d\n", __FILE__, __LINE__);                       \
        kprintf(__VA_ARGS__);                                            \
        kprintf("\n========================================\n");         \
        while (1)                                                        \
            ;                                                            \
    } while (0)

#define panic_no_print(...)      \
    do {                         \
        irq_disable();           \
        while (1)                \
            ;                    \
    } while (0)

#else

#include <stdio.h>
#include <assert.h>

#define panic(...)                                                      \
    do {                                                                \
        fprintf(stderr, "=========== KERNEL PANIC :^( ===========\n");  \
        fprintf(stderr, "At %s:%d\n", __FILE__, __LINE__);              \
        fprintf(stderr, __VA_ARGS__);                                   \
        fprintf(stderr, "\n========================================\n");\
        assert(false);                                                  \
    } while(0);

#define panic_no_print(...) panic(__VA_ARGS__)

#endif

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

#define todo() panic("TODO")
