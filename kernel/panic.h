#pragma once

#include <stddef.h>

namespace kernel {
size_t kprintf(char const* format, ...);
}

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

#define FORMAT_TASK_STATE                               \
    "\t r0: %x\t r1: %x\t r2: %x\t r3: %x\n"            \
    "\t r4: %x\t r5: %x\t r6: %x\t r7: %x\n"            \
    "\t r8: %x\t r9: %x\t r10: %x\t r11: %x\n"          \
    "\t sp: %p\n"                                       \
    "\t user lr: %p\n"                                  \
    "\t spsr: %x"

#define FORMAT_ARGS_TASK_STATE(state)                   \
    (state)->r[0],  (state)->r[1],                      \
    (state)->r[2],  (state)->r[3],                      \
    (state)->r[4],  (state)->r[5],                      \
    (state)->r[6],  (state)->r[7],                      \
    (state)->r[8],  (state)->r[9],                      \
    (state)->r[10], (state)->r[11],                     \
    (state)->task_sp,                                   \
    (state)->task_lr,                                   \
    (state)->spsr

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
