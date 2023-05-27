#pragma once

#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <stdint.h>

namespace kernel {

typedef void (*SystimerCallback)(SuspendedTaskState*);

void systimer_init();

uint64_t systimer_get_ticks();

Error systimer_repeating_callback(uint32_t interval, SystimerCallback callback);

Error systimer_exec_after(uint64_t ticks, SystimerCallback callback);

}
