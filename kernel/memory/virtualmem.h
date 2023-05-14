#pragma once

#include <kernel/error.h>

namespace kernel {

uintptr_t virt2phys(uintptr_t virt);

void mmu_prepare_kernel_address_space();

Error mmu_map_framebuffer(uint32_t*&, uintptr_t, size_t);

enum class VirtualSectionType {
    KernelHeap
};
Error mmu_map_section(uintptr_t, uintptr_t, VirtualSectionType);

Error mmu_unmap_section(uintptr_t);

}
