#pragma once

#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <kernel/memory/vm.h>

namespace kernel {

typedef uint32_t PID;

struct Task {
    AddressSpace address_space;
    SuspendedTaskState state;
    char name[32];
    PID pid;
    Task* next_to_run;
};

void scheduler_init();

void scheduler_add(Task* task);

[[noreturn]] void scheduler_begin();

Task* scheduler_current_task();

void yield();

Error task_create_kernel_thread(Task*&, char const* name, void (*entry)());

void scheduler_step(SuspendedTaskState*);

}
