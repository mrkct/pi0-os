#pragma once

#include <kernel/error.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/armv6mmu.h>
#include <kernel/memory/physicalalloc.h>

namespace kernel {

struct AddressSpace {
    struct PhysicalPage* ttbr0_page;
};

struct AddressSpace& vm_current_address_space();

uintptr_t virt2phys(uintptr_t virt);

Error vm_init_kernel_address_space();

Error vm_create_address_space(struct AddressSpace&);

Error vm_map(struct AddressSpace&, struct PhysicalPage*, uintptr_t);

Error vm_map_mmio(struct AddressSpace&, uintptr_t phys_addr, uintptr_t virt_addr, size_t size);

Error vm_unmap(struct AddressSpace&, uintptr_t, struct PhysicalPage*&);

Error vm_copy_from_user(struct AddressSpace&, void* dest, uintptr_t src, size_t len);

Error vm_copy_to_user(struct AddressSpace& as, uintptr_t dest, void const* src, size_t len);

Error vm_memset(struct AddressSpace&, uintptr_t dest, uint8_t val, size_t size);

void vm_switch_address_space(struct AddressSpace&);

template<typename Callback>
auto vm_using_address_space(struct AddressSpace& as, Callback c)
{
    auto& previous = vm_current_address_space();
    vm_switch_address_space(as);
    auto result = c();
    vm_switch_address_space(previous);

    return result;
}

enum class PageFaultHandlerResult {
    Fixed,
    Fatal
};
PageFaultHandlerResult vm_page_fault_handler(uintptr_t phys_ttbr0_addr, uintptr_t fault_addr, uintptr_t status);

}
