#include <kernel/lib/math.h>
#include <kernel/memory/vm.h>
#include <kernel/sizes.h>
#include <kernel/lib/libc/string.h>


namespace kernel {

static constexpr size_t LVL1_ENTRIES = 16 * _16KB / sizeof(FirstLevelEntry);
static constexpr size_t LVL2_ENTRIES = _1KB / sizeof(SecondLevelEntry);

extern "C" FirstLevelEntry _kernel_translation_table[];
static __attribute__((aligned(_1KB))) SecondLevelEntry g_temp_mappings_lvl2_table[LVL2_ENTRIES];

class TemporarilyMappedRange {
public:
    TemporarilyMappedRange(uintptr_t phys_addr, size_t size)
    {
        // The below is to handle the case where we want to map a 1KB page that is not aligned to 4KB
        auto offset_since_page_start = phys_addr % _4KB;
        phys_addr -= offset_since_page_start;
        size += offset_since_page_start;

        auto consecutive_entries_required = klib::round_up<size_t>(size, _4KB) / _4KB;
        auto idx = try_find_potential_mapping_place(consecutive_entries_required);
        if (idx == LVL2_ENTRIES)
            panic("TemporarilyMappedPage: no place for mapping");

        for (size_t i = 0; i < consecutive_entries_required; i++) {
            auto& entry = g_temp_mappings_lvl2_table[idx + i];
            entry.small_page = SmallPageEntry::make_entry(phys_addr + i * 4 * _1KB);
            invalidate_tlb_entry(areas::temp_mappings.start + (idx + i) * 4 * _1KB);
        }
        m_stored_range_start = idx;
        m_stored_range_end = idx + consecutive_entries_required - 1;
        m_virtual_addr = reinterpret_cast<void*>(areas::temp_mappings.start + idx * 4 * _1KB + offset_since_page_start);
    }

    template<typename T>
    T as_ptr() { return reinterpret_cast<T>(m_virtual_addr); }

    ~TemporarilyMappedRange()
    {
        for (size_t i = m_stored_range_start; i <= m_stored_range_end; i++) {
            auto& entry = g_temp_mappings_lvl2_table[i];
            entry.raw = 0;
            invalidate_tlb_entry(areas::temp_mappings.start + i * 4 * _1KB);
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

    auto &lvl1_entry = _kernel_translation_table[lvl1_index(virt)];
    switch (lvl1_entry.section.identifier) {
    case 0:
        return 0;
    case SECTION_ENTRY_ID:
        return lvl1_entry.section.base_address() | (virt & 0x000fffff);
    case COARSE_PAGE_TABLE_ENTRY_ID: {
        TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };
        auto &lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt)];
        return lvl2_entry.small_page.base_address() | (virt & 0x00000fff);
    }
    default:
        panic("virt2phys: unknown page table entry type, wtf?");
    }
}

Error vm_init_kernel_address_space()
{
    // Note that we already mapped the kernel and peripherals in the start.S file
    _kernel_translation_table[lvl1_index(areas::temp_mappings.start)].coarse = CoarsePageTableEntry::make_entry(
        virt2phys(reinterpret_cast<uintptr_t>(&g_temp_mappings_lvl2_table))
    );

    // This value depends on the address at which the kernel is mapped
    // eg: 0xe0000000 has the first 3 bits set to 1, therefore N=3
    auto const N = 3;
    asm volatile("mcr p15, 0, %0, c2, c0, 2" ::"r"(N)); // Write N to TTBCR, the remaining bits are zero

    invalidate_tlb();

    return Success;
}

Error vm_create_address_space(struct AddressSpace& as)
{
    struct PhysicalPage* as_ttbr1_page;
    TRY(physical_page_alloc(PageOrder::_16KB, as_ttbr1_page));
    as.ttbr1_page = as_ttbr1_page;

    TemporarilyMappedRange tmp_mapped{ page2addr(as_ttbr1_page), LVL1_TABLE_SIZE };
    memset(tmp_mapped.as_ptr<void*>(), 0, LVL1_TABLE_SIZE);

    return Success;
}

static Error vm_map_kernel_page(uintptr_t phys_addr, uintptr_t virt_addr)
{
    kassert(areas::higher_half.contains(virt_addr));

    auto &lvl1_entry = _kernel_translation_table[lvl1_index(virt_addr)];
    if (lvl1_entry.section.identifier == SECTION_ENTRY_ID)
        panic("vm_map_kernel: Address %p is already mapped to a section", virt_addr);
    
    bool lvl2_table_was_just_allocated = false;
    if (lvl1_entry.raw == 0) {
        struct PhysicalPage *lvl2_table_page;
        MUST(physical_page_alloc(PageOrder::_1KB, lvl2_table_page));

        lvl1_entry.coarse = CoarsePageTableEntry::make_entry(page2addr(lvl2_table_page));
        lvl2_table_was_just_allocated = true;
    }

    TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };

    if (lvl2_table_was_just_allocated)
        memset(lvl2_table.as_ptr<void*>(), 0, LVL2_TABLE_SIZE);
    
    auto &lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt_addr)];
    if (lvl2_entry.raw != 0)
        panic("vm_map_kernel: mapping already exists at %p (currenly mapped to %p)", virt_addr, lvl2_entry.small_page.base_address());

    lvl2_entry.small_page = SmallPageEntry::make_entry(phys_addr);

    invalidate_tlb_entry(virt_addr);
    return Success;
}

static Error vm_map_page(struct AddressSpace& as, uintptr_t phys_addr, uintptr_t virt_addr)
{
    if (areas::higher_half.contains(virt_addr))
        return vm_map_kernel_page(phys_addr, virt_addr);

    TemporarilyMappedRange lvl1_table { page2addr(as.ttbr1_page), LVL1_TABLE_SIZE };

    bool lvl2_table_was_just_allocated = false;
    auto& lvl1_entry = lvl1_table.as_ptr<FirstLevelEntry*>()[lvl1_index(virt_addr)];
    if (lvl1_entry.raw == 0) {
        struct PhysicalPage *lvl2_table_page;
        TRY(physical_page_alloc(PageOrder::_1KB, lvl2_table_page));
        lvl1_entry.coarse = CoarsePageTableEntry::make_entry(page2addr(lvl2_table_page));
        lvl2_table_was_just_allocated = true;
    }

    TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };
    
    if (lvl2_table_was_just_allocated)
        memset(lvl2_table.as_ptr<void*>(), 0, LVL2_TABLE_SIZE);
    
    auto& lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt_addr)];
    if (lvl2_entry.raw != 0)
        panic("vm_map: mapping already exists at %p (currenly mapped to %p)", virt_addr, lvl2_entry.small_page.base_address());

    lvl2_entry.small_page = SmallPageEntry::make_entry(phys_addr);
    invalidate_tlb_entry(virt_addr);

    return Success;
}

Error vm_map(struct AddressSpace& as, struct PhysicalPage* page, uintptr_t virt_addr)
{
    TRY(vm_map_page(as, page2addr(page), virt_addr));
    page->ref_count++;
    return Success;
}

Error vm_map_mmio(struct AddressSpace& as, uintptr_t phys_addr, uintptr_t virt_addr, size_t size)
{
    auto pages_to_map = klib::round_up<size_t>(size, _4KB) / _4KB;
    for (size_t i = 0; i < pages_to_map; i++) {
        TRY(vm_map_page(as, phys_addr + i * _4KB, virt_addr + i * _4KB));
    }

    return Success;
}

static Error vm_unmap_kernel_page(uintptr_t virt_addr, uintptr_t& previously_mapped_physical_address)
{
    kassert(areas::higher_half.contains(virt_addr));

    if (areas::temp_mappings.contains(virt_addr))
        panic("vm_unmap_kernel: Address %p is in the temporary mappings area, you can't unmap that!", virt_addr);

    auto &lvl1_entry = _kernel_translation_table[lvl1_index(virt_addr)];
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

    return Success;
}

static Error vm_unmap_page(struct AddressSpace& as, uintptr_t virt_addr, uintptr_t& previously_mapped_physical_address)
{
    if (areas::higher_half.contains(virt_addr))
        return vm_unmap_kernel_page(virt_addr, previously_mapped_physical_address);

    TemporarilyMappedRange lvl1_table { page2addr(as.ttbr1_page), LVL1_TABLE_SIZE };

    auto& lvl1_entry = lvl1_table.as_ptr<FirstLevelEntry*>()[lvl1_index(virt_addr)];
    if (lvl1_entry.raw == 0)
        return Success;

    TemporarilyMappedRange lvl2_table { lvl1_entry.coarse.base_address(), LVL2_TABLE_SIZE };
    auto& lvl2_entry = lvl2_table.as_ptr<SecondLevelEntry*>()[lvl2_index(virt_addr)];
    if (lvl2_entry.raw == 0)
        return Success;

    previously_mapped_physical_address = lvl2_entry.small_page.base_address() << 12;
    lvl2_entry.raw = 0;
    invalidate_tlb_entry(virt_addr);

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

static AddressSpace g_current_address_space;

struct AddressSpace& vm_current_address_space()
{
    return g_current_address_space;
}

void vm_switch_address_space(struct AddressSpace& as)
{
    asm volatile("mcr p15, 0, %0, c2, c0, 0" ::"r"(page2addr(as.ttbr1_page) | 0b11));
    invalidate_tlb();
    g_current_address_space = as;
}

}
