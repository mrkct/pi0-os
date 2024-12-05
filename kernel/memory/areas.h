#pragma once

#include <kernel/base.h>


namespace areas {

// These are defined by the bootloader
static constexpr uintptr_t KERNEL_VIRT_START_ADDR = 0xc0000000;
static constexpr uintptr_t PHYS_MEM_START_ADDR =    0xe0000000;

static constexpr Range kernel_area = { KERNEL_VIRT_START_ADDR, 0xffffffff };
static constexpr Range kernel_code = Range::from_start_and_size(kernel_area.start, 16 * _1MB);
static constexpr Range peripherals = Range::from_start_and_size(kernel_code.end, 32 * _1MB);
static constexpr Range kernel_heap = Range {peripherals.end, PHYS_MEM_START_ADDR};
static constexpr Range physical_mem = Range::from_start_and_size(PHYS_MEM_START_ADDR, 512 * _1MB);

}
