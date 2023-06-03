#pragma once

#include <kernel/error.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/armv6mmu.h>
#include <kernel/memory/physicalalloc.h>

namespace kernel {

struct AddressSpace {
    struct PhysicalPage* ttbr1_page;
};

struct AddressSpace& vm_current_address_space();

uintptr_t virt2phys(uintptr_t virt);

Error vm_init_kernel_address_space();

Error vm_create_address_space(struct AddressSpace&);

Error vm_map(struct AddressSpace&, struct PhysicalPage*, uintptr_t);

Error vm_map_mmio(struct AddressSpace&, uintptr_t phys_addr, uintptr_t virt_addr, size_t size);

Error vm_unmap(struct AddressSpace&, uintptr_t, struct PhysicalPage*&);

void vm_switch_address_space(struct AddressSpace&);

}
