#include <kernel/lib/math.h>
#include <kernel/lib/memory.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/armv6mmu.h>
#include <kernel/memory/sectionalloc.h>
#include <kernel/memory/virtualmem.h>

namespace kernel {

static __attribute__((aligned(1024))) SmallPageEntry g_temp_mappings_lvl2_table[1024] = {};

extern "C" FirstLevelEntry _kernel_translation_table[];

uintptr_t virt2phys(uintptr_t virt)
{
    if (areas::kernel.contains(virt))
        return virt - areas::kernel.start;
    else if (areas::peripherals.contains(virt)) {
        // 0x20000000 is the base address of the BCM2835 peripherals
        return virt - areas::peripherals.start + 0x20000000;
    }

    switch (_kernel_translation_table[lvl1_index(virt)].section.identifier) {
    case 0:
        return 0;
    case SECTION_ENTRY_ID:
        return _kernel_translation_table[lvl1_index(virt)].section.base_address() | (virt & 0x000fffff);
    case COARSE_PAGE_TABLE_ENTRY_ID:
        panic("virt2phys: coarse page table entries are not supported yet :^(");
    default:
        panic("virt2phys: unknown page table entry type, wtf?");
    }
}

void mmu_prepare_kernel_address_space()
{
    // Note that we already mapped the kernel and peripherals in the start.S file
    _kernel_translation_table[lvl1_index(areas::temp_mappings.start)].coarse = CoarsePageTableEntry {
        .identifier = COARSE_PAGE_TABLE_ENTRY_ID,
        .sbz = 0,
        .domain = 0,
        .impl_defined = 0,
        .base_addr = (virt2phys(reinterpret_cast<uintptr_t>(&g_temp_mappings_lvl2_table)) >> 10) & 0x3fffff,
    };

    // This value depends on the address at which the kernel is mapped
    // eg: 0xe0000000 has the first 3 bits set to 1, therefore N=3
    auto const N = 3;
    asm volatile("mcr p15, 0, %0, c2, c0, 2" ::"r"(N)); // Write N to TTBCR, the remaining bits are zero

    invalidate_tlb();
}

Error mmu_map_framebuffer(uint32_t*& virt_addr, uintptr_t addr, size_t size)
{
    auto idx = lvl1_index(areas::framebuffer.start);
    if (_kernel_translation_table[idx].raw != 0)
        return CantRepeat;

    if (addr % 1024 * 1024 != 0)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Framebuffer physical address is not aligned to 1MB",
            .extra_data = nullptr
        };

    size = klib::round_up<size_t>(size, 1024 * 1024);
    if (size > areas::framebuffer.end - areas::framebuffer.start)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Framebuffer size is too large, extend the framebuffer area in the code!",
            .extra_data = nullptr
        };

    auto mbs_to_map = size / (1024 * 1024);
    for (size_t i = 0; i < mbs_to_map; i++) {
        _kernel_translation_table[idx + i].section = SectionEntry {
            .identifier = SECTION_ENTRY_ID,
            .bufferable_writes = 0,
            .cachable = 0,
            .sbz = 0,
            .domain = 0,
            .impl_defined = 0,
            .access_permission = 0b000,
            .tex = 0b000,
            .sbz2 = 0,
            .base_addr = ((addr + 1024 * 1024 * i) >> 20) & 0xfff,
        };
    }

    virt_addr = reinterpret_cast<uint32_t*>(areas::framebuffer.start);
    invalidate_tlb();

    return Success;
}

Error mmu_map_section(uintptr_t phys, uintptr_t virt, VirtualSectionType)
{
    if (virt % SECTION_SIZE != 0)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Virtual address is not aligned to 1MB",
            .extra_data = nullptr
        };

    if (phys % SECTION_SIZE != 0)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Physical address is not aligned to 1MB",
            .extra_data = nullptr
        };

    auto idx = lvl1_index(virt);
    if (_kernel_translation_table[idx].raw != 0)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Virtual address is already mapped",
            .extra_data = nullptr
        };

    _kernel_translation_table[idx].section = SectionEntry {
        .identifier = SECTION_ENTRY_ID,
        .bufferable_writes = 0,
        .cachable = 0,
        .sbz = 0,
        .domain = 0,
        .impl_defined = 0,
        .access_permission = 0b011,
        .tex = 0b001,
        .sbz2 = 0,
        .base_addr = (phys >> 20) & 0xfff,
    };

    invalidate_tlb();

    return Success;
}

Error mmu_unmap_section(uintptr_t virt)
{
    if (virt % SECTION_SIZE != 0)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Virtual address is not aligned to 1MB",
            .extra_data = nullptr
        };

    if (_kernel_translation_table[lvl1_index(virt)].section.identifier != SECTION_ENTRY_ID)
        return Error {
            .generic_error_code = GenericErrorCode::BadParameters,
            .device_specific_error_code = 0,
            .user_message = "Virtual address is not mapped",
            .extra_data = nullptr
        };

    _kernel_translation_table[lvl1_index(virt)].raw = 0;
    return Success;
}

}
