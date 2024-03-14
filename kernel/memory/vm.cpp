#include <kernel/lib/string.h>
#include <kernel/lib/math.h>
#include <kernel/memory/vm.h>
#include <kernel/sizes.h>

namespace kernel {

static constexpr size_t LVL1_ENTRIES = _16KB / sizeof(FirstLevelEntry);
static constexpr size_t LVL2_ENTRIES = _1KB / sizeof(SecondLevelEntry);

extern "C" FirstLevelEntry _kernel_translation_table[];
static __attribute__((aligned(_1KB))) SecondLevelEntry g_temp_mappings_lvl2_table[LVL2_ENTRIES];

static AddressSpace g_kernel_address_space;
static AddressSpace g_current_address_space;

struct AddressSpace& vm_current_address_space()
{
    return g_current_address_space;
}

struct AddressSpace& vm_kernel_address_space()
{
    return g_kernel_address_space;
}

void vm_switch_address_space(struct AddressSpace& as)
{
    asm volatile("mcr p15, 0, %0, c2, c0, 0" ::"r"(page2addr(as.ttbr0_page)));
    g_current_address_space = as;
    invalidate_tlb();
}

uintptr_t vm_read_current_ttbr0()
{
    uintptr_t ttbr0;
    asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr0));
    return ttbr0;
}

class TemporarilyMappedRange {
public:
    TemporarilyMappedRange(uintptr_t phys_addr, size_t size)
    {
        // The below is to handle the case where we want to map a 1KB page that is not aligned to 4KB
        auto offset_since_page_start = phys_addr % _4KB;
        phys_addr -= offset_since_page_start;
        size += offset_since_page_start;

        auto consecutive_entries_required = round_up<size_t>(size, _4KB) / _4KB;
        auto idx = try_find_potential_mapping_place(consecutive_entries_required);
        if (idx == LVL2_ENTRIES)
            panic("TemporarilyMappedPage: no place for mapping");

        for (size_t i = 0; i < consecutive_entries_required; i++) {
            auto& entry = g_temp_mappings_lvl2_table[idx + i];
            entry.small_page = SmallPageEntry::make_entry(phys_addr + i * _4KB, PageAccessPermissions::PriviledgedOnly);
            invalidate_tlb_entry(areas::temp_mappings.start + (idx + i) * _4KB);
        }
        m_stored_range_start = idx;
        m_stored_range_end = idx + consecutive_entries_required - 1;
        m_virtual_addr = reinterpret_cast<void*>(areas::temp_mappings.start + idx * _4KB + offset_since_page_start);
    }

    template<typename T>
    T as_ptr() { return reinterpret_cast<T>(m_virtual_addr); }

    ~TemporarilyMappedRange()
    {
        for (size_t i = m_stored_range_start; i <= m_stored_range_end; i++) {
            auto& entry = g_temp_mappings_lvl2_table[i];
            entry.raw = 0;
            invalidate_tlb_entry(areas::temp_mappings.start + i * _4KB);
        }
    }

private:
    size_t try_find_potential_mapping_place(size_t consecutive_entries)
    {
        for (size_t i = 0; i < LVL2_ENTRIES - consecutive_entries; i++) {

            bool all_entries_are_free = true;
            for (size_t j = 0; j < consecutive_entries; j++) {
                auto const& entry = g_temp_mappings_lvl2_table[i + j];
                if (entry.raw != 0) {
                    all_entries_are_free = false;
                    break;
                }
            }

            if (all_entries_are_free)
                return i;
        }

        return LVL2_ENTRIES;
    }

    size_t m_stored_range_start;
    size_t m_stored_range_end;
    void* m_virtual_addr;
};

uintptr_t virt2phys(uintptr_t virt)
{
    if (areas::kernel.contains(virt))
        return virt - areas::kernel.start;
    else if (areas::peripherals.contains(virt)) {
        // 0x20000000 is the base address of the BCM2835 peripherals
        return virt - areas::peripherals.start + 0x20000000;
    }

    auto& lvl1_entry = _kernel_translation_table[lvl1_index(virt)];
    switch (lvl1_entry.section.identifier) {
    case 0:
        return 0;
    case SECTION_ENTRY_ID:
        return lvl1_entry.section.base_address() | (virt & 0x000fffff);
    case COARSE_PAGE_TABLE_ENTRY_ID: {
        TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };
        auto& lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt)];
        return lvl2_entry.small_page.base_address() | (virt & 0x00000fff);
    }
    default:
        panic("virt2phys: unknown page table entry type, wtf?");
    }
}

Error vm_init_kernel_address_space()
{
    g_current_address_space.ttbr0_page = addr2page(virt2phys(reinterpret_cast<uintptr_t>(_kernel_translation_table)));
    g_kernel_address_space = g_current_address_space;

    // Note that we already mapped the kernel and peripherals in the start.S file
    _kernel_translation_table[lvl1_index(areas::temp_mappings.start)].coarse = CoarsePageTableEntry::make_entry(
        virt2phys(reinterpret_cast<uintptr_t>(&g_temp_mappings_lvl2_table)));

    invalidate_tlb();

    return Success;
}

Error vm_create_address_space(struct AddressSpace& as)
{
    struct PhysicalPage* as_ttbr0_page;
    TRY(physical_page_alloc(PageOrder::_16KB, as_ttbr0_page));
    as.ttbr0_page = as_ttbr0_page;

    TemporarilyMappedRange tmp_mapped { page2addr(as_ttbr0_page), LVL1_TABLE_SIZE };
    FirstLevelEntry* lvl1_table = tmp_mapped.as_ptr<FirstLevelEntry*>();
    memset(lvl1_table, 0, LVL1_TABLE_SIZE);

    constexpr uintptr_t KERNEL_START = areas::higher_half.start;
    for (size_t i = lvl1_index(KERNEL_START); i < LVL1_ENTRIES; i++)
        lvl1_table[i].raw = _kernel_translation_table[i].raw;

    // Map the first 1MB of virtual memory to the first 1MB of physical memory
    // This is necessary because the CPU jumps to the vector table, which is located there and
    // it must be always be accessible or we risk a fault loop (the CPU will try to jump to the
    // vector table, but it will fault because it's not mapped, so it will try to jump to the
    // vector table and so on)
    lvl1_table[0].section = SectionEntry::make_entry(0x00000000, PageAccessPermissions::PriviledgedOnly);

    return Success;
}

static Error vm_map_page(FirstLevelEntry* root_table, uintptr_t phys_addr, uintptr_t virt_addr, PageAccessPermissions permissions)
{
    auto& lvl1_entry = root_table[lvl1_index(virt_addr)];
    if (lvl1_entry.section.identifier == SECTION_ENTRY_ID)
        panic("vm_map_page: Address %p is already mapped to a section", virt_addr);

    bool lvl2_table_was_just_allocated = false;
    if (lvl1_entry.raw == 0) {
        struct PhysicalPage* lvl2_table_page;
        MUST(physical_page_alloc(PageOrder::_1KB, lvl2_table_page));

        lvl1_entry.coarse = CoarsePageTableEntry::make_entry(page2addr(lvl2_table_page));

        // The kernel area must be mapped the same way in all address spaces.
        // We can afford to have different address spaces not mapped the same way since
        // we can fix them in the page fault handler anyway, but we must always have the
        // kernel_translation_table be the final source of truth for the kernel area.
        if (areas::higher_half.contains(virt_addr) && root_table != _kernel_translation_table) {
            kassert(_kernel_translation_table[lvl1_index(virt_addr)].raw == 0);
            _kernel_translation_table[lvl1_index(virt_addr)].coarse = lvl1_entry.coarse;
        }

        lvl2_table_was_just_allocated = true;
    }

    TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };

    if (lvl2_table_was_just_allocated)
        memset(lvl2_table.as_ptr<void*>(), 0, LVL2_TABLE_SIZE);

    auto& lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt_addr)];
    if (lvl2_entry.raw != 0)
        panic("vm_map_page: mapping already exists at %p (currenly mapped to %p)", virt_addr, lvl2_entry.small_page.base_address());

    lvl2_entry.small_page = SmallPageEntry::make_entry(phys_addr, permissions);

    invalidate_tlb_entry(virt_addr);
    return Success;
}

static Error vm_map_page(struct AddressSpace& as, uintptr_t phys_addr, uintptr_t virt_addr, PageAccessPermissions permissions)
{
    // Fast path for the kernel address space
    if (as.ttbr0_page == g_kernel_address_space.ttbr0_page)
        return vm_map_page(_kernel_translation_table, phys_addr, virt_addr, permissions);
    
    TemporarilyMappedRange lvl1_table { page2addr(as.ttbr0_page), LVL1_TABLE_SIZE };
    TRY(vm_map_page(lvl1_table.as_ptr<FirstLevelEntry*>(), phys_addr, virt_addr, permissions));

    return Success;
}

Error vm_map(struct AddressSpace& as, struct PhysicalPage* page, uintptr_t virt_addr, PageAccessPermissions permissions)
{
    TRY(vm_map_page(as, page2addr(page), virt_addr, permissions));
    page->ref_count++;
    return Success;
}

Error vm_map_mmio(struct AddressSpace& as, uintptr_t phys_addr, uintptr_t virt_addr, size_t size)
{
    auto pages_to_map = round_up<size_t>(size, _4KB) / _4KB;
    for (size_t i = 0; i < pages_to_map; i++) {
        TRY(vm_map_page(as, phys_addr + i * _4KB, virt_addr + i * _4KB, PageAccessPermissions::PriviledgedOnly));
    }

    return Success;
}

static Error vm_unmap_page(FirstLevelEntry* root_table, uintptr_t virt_addr, uintptr_t& previously_mapped_physical_address)
{
    if (areas::temp_mappings.contains(virt_addr))
        panic("vm_unmap_kernel: Address %p is in the temporary mappings area, you can't unmap that!", virt_addr);

    auto& lvl1_entry = root_table[lvl1_index(virt_addr)];
    if (lvl1_entry.section.identifier == SECTION_ENTRY_ID)
        panic("vm_unmap_kernel: Address %p is mapped to a section, you can't unmap that!", virt_addr);

    if (lvl1_entry.raw == 0) {
        previously_mapped_physical_address = 0;
        return Success;
    }

    TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };
    auto& lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt_addr)];
    if (lvl2_entry.raw == 0) {
        previously_mapped_physical_address = 0;
        return Success;
    }

    previously_mapped_physical_address = lvl2_entry.small_page.base_address() << 12;
    lvl2_entry.raw = 0;
    invalidate_tlb_entry(virt_addr);

    // If the whole level 2 table is empty, and it's not a kernel area address, we can free it
    // Note we don't want to unmap lvl1 tables in the kernel address space because
    // it might cause hard-to-solve inconsistencies with the other address spaces
    if (!areas::higher_half.contains(virt_addr)) {
        bool whole_lvl2_table_is_empty = true;
        for (size_t i = 0; i < LVL2_ENTRIES; i++) {
            auto& entry = lvl2_table.as_ptr<SecondLevelEntry*>()[i];
            if (entry.raw != 0) {
                whole_lvl2_table_is_empty = false;
                break;
            }
        }
        if (whole_lvl2_table_is_empty) {
            struct PhysicalPage* p = addr2page(lvl1_entry.coarse.base_address());
            MUST(physical_page_free(p, PageOrder::_16KB));
            lvl1_entry.raw = 0;
        }
    }

    return Success;
}

static Error vm_unmap_page(struct AddressSpace& as, uintptr_t virt_addr, uintptr_t& previously_mapped_page)
{
    if (areas::higher_half.contains(virt_addr))
        return vm_unmap_page(_kernel_translation_table, virt_addr, previously_mapped_page);

    TemporarilyMappedRange lvl1_table { page2addr(as.ttbr0_page), LVL1_TABLE_SIZE };
    TRY(vm_unmap_page(lvl1_table.as_ptr<FirstLevelEntry*>(), virt_addr, previously_mapped_page));

    return Success;
}

Error vm_unmap(struct AddressSpace& as, uintptr_t virt_addr, struct PhysicalPage*& previously_mapped_page)
{
    uintptr_t previously_mapped_physical_address;

    TRY(vm_unmap_page(as, virt_addr, previously_mapped_physical_address));
    previously_mapped_page = addr2page(previously_mapped_physical_address);
    previously_mapped_page->ref_count--;

    return Success;
}

Error vm_copy_from_user(struct AddressSpace& as, void* dest, uintptr_t src, size_t len)
{
    if (g_current_address_space.ttbr0_page == as.ttbr0_page) {
        memcpy(dest, reinterpret_cast<void*>(src), len);
        return Success;
    }

    TODO();

    return Success;
}

Error vm_copy_to_user(struct AddressSpace& as, uintptr_t dest, void const* src, size_t len)
{
    if (g_current_address_space.ttbr0_page == as.ttbr0_page) {
        memcpy(reinterpret_cast<void*>(dest), src, len);
        return Success;
    }

    TODO();

    return Success;
}

Error vm_memset(struct AddressSpace& as, uintptr_t dest, uint8_t val, size_t size)
{
    if (g_current_address_space.ttbr0_page == as.ttbr0_page) {
        memset(reinterpret_cast<void*>(dest), val, size);
        return Success;
    }

    TODO();

    return Success;
}

PageFaultHandlerResult vm_try_fix_page_fault(uintptr_t fault_addr)
{
    uintptr_t phys_ttbr0_addr = vm_read_current_ttbr0();
    if (phys_ttbr0_addr == virt2phys(reinterpret_cast<uintptr_t>(_kernel_translation_table)))
        return PageFaultHandlerResult::KernelFatal;

    // We don't implement any sort of swap memory, so this is an error in the application for sure
    if (!areas::higher_half.contains(fault_addr))
        return PageFaultHandlerResult::ProcessFatal;

    if (_kernel_translation_table[lvl1_index(fault_addr)].raw == 0)
        return PageFaultHandlerResult::KernelFatal;
    
    FirstLevelEntry kernel_lvl1_entry = _kernel_translation_table[lvl1_index(fault_addr)];
    TemporarilyMappedRange ttbr0 { phys_ttbr0_addr, LVL1_TABLE_SIZE };
    auto& ttbr0_lvl1_entry = ttbr0.as_ptr<FirstLevelEntry*>()[lvl1_index(fault_addr)];

    // If the fault is due to a missing lvl1 table in the kernel area, but the main
    // kernel address space has a valid entry, then it's just that the process' address
    // space was created before the new mapping was added
    if (ttbr0_lvl1_entry.raw == 0) {
        ttbr0_lvl1_entry = kernel_lvl1_entry;
        invalidate_tlb_entry(fault_addr);
        return PageFaultHandlerResult::Fixed;
    }

    return PageFaultHandlerResult::KernelFatal;
}

}
