#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

void arm_timer_init();

void arm_timer_exec_after(uint32_t microseconds, void (*callback)());

}
