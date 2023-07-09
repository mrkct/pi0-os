#pragma once

#include <stdint.h>

namespace kernel {

void syscall_init();

void dispatch_syscall(uint32_t& r7, uint32_t& r0, uint32_t& r1, uint32_t& r2, uint32_t& r3, uint32_t& r4);

}
