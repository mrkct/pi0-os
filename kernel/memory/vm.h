#pragma once

#include <kernel/base.h>
#include <kernel/memory/areas.h>
#include <kernel/boot/boot.h>
#include <kernel/arch/arm/armv6mmu.h>
#include <kernel/interrupt.h>
#include <kernel/memory/physicalalloc.h>


uintptr_t virt2phys(uintptr_t virt);

uintptr_t phys2virt(uintptr_t phys);


struct AddressSpace {
    struct PhysicalPage* ttbr0_page;
    FirstLevelEntry *get_root_table_ptr() const { return reinterpret_cast<FirstLevelEntry*>(phys2virt(page2addr(ttbr0_page))); }
};

void vm_early_init(BootParams const *boot_params);

void vm_init();

void *ioremap(uintptr_t phys_addr, size_t size);

struct AddressSpace& vm_current_address_space();

struct AddressSpace& vm_kernel_address_space();

uintptr_t vm_read_current_ttbr0();


Error vm_init_kernel_address_space();

Error vm_create_address_space(struct AddressSpace&);

Error vm_map(struct AddressSpace&, struct PhysicalPage*, uintptr_t, PageAccessPermissions);

Error vm_map_mmio(struct AddressSpace&, uintptr_t phys_addr, uintptr_t virt_addr, size_t size);

Error vm_unmap(struct AddressSpace&, uintptr_t, struct PhysicalPage*&);

Error vm_copy_from_user(struct AddressSpace&, void* dest, uintptr_t src, size_t len);

Error vm_copy_to_user(struct AddressSpace& as, uintptr_t dest, void const* src, size_t len);

Error vm_memset(struct AddressSpace&, uintptr_t dest, uint8_t val, size_t size);

void vm_switch_address_space(struct AddressSpace&);

void vm_free(struct AddressSpace&);

template<typename Callback>
auto vm_using_address_space(struct AddressSpace& as, Callback c)
{
    auto previous = vm_current_address_space();
    vm_switch_address_space(as);
    auto result = c();
    vm_switch_address_space(previous);

    return result;
}

enum class PageFaultHandlerResult {
    Fixed,
    ProcessFatal,
    KernelFatal
};
PageFaultHandlerResult vm_try_fix_page_fault(uintptr_t fault_addr);
