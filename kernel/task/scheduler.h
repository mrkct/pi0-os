#pragma once

#include <kernel/interrupt.h>

namespace kernel {

struct Task {
    SuspendedTaskState state;
    char name[32];
    uint32_t pid;
    Task* next_to_run;
};

void scheduler_init();

void scheduler_add(Task* task);

[[noreturn]] void scheduler_begin();

Task* scheduler_current_task();

void yield();

Error task_create_kernel_thread(Task*&, char const* name, void (*entry)());

}
