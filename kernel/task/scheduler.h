#pragma once

#include <kernel/interrupt.h>
#include <kernel/task/task.h>

namespace kernel {

void scheduler_init();

void scheduler_add(Task* task);

void scheduler_step();

}
