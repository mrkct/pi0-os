#pragma once

#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct Task {
    VectorFrame vector_frame;
    uint32_t sp;

    char name[32];
    uint32_t pid;

    Task* next_to_run;
};

Error task_create_kernel_thread(Task*&, char const* name, void (*entry)());

void context_switch_kernel_threads(Task* current, Task* to);

}
