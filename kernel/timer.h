#pragma once

#include <kernel/error.h>

namespace kernel {

void timer_init();

uint64_t timer_time_passed_since_boot_in_ms();

Error timer_exec_after(uint32_t ms, void (*callback)(void*), void*);

}
