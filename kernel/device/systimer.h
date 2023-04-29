#pragma once

#include <kernel/error.h>
#include <stdint.h>

namespace kernel {

void systimer_init();

uint64_t systimer_get_ticks();

Error systimer_exec_after(uint64_t ticks, void (*callback)());

}
