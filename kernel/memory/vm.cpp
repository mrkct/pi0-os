#include <kernel/base.h>
#include "vm.h"


static constexpr size_t LVL1_ENTRIES = _16KB / sizeof(FirstLevelEntry);
static constexpr size_t LVL2_ENTRIES = _1KB / sizeof(SecondLevelEntry);

static AddressSpace g_kernel_address_space;
static AddressSpace g_current_address_space;
static struct {
    // TODO: Use a bitmap instead of an array
    uint8_t used[areas::peripherals.size() / _4KB];
} s_ioremap;
static struct {
    uintptr_t phys_start_addr;
    uint32_t size;
} s_ram;

enum class InitState {
    None, Early, Completed
};
static InitState s_init_state = InitState::None;

void vm_early_init(BootParams const *boot_params)
{
    s_ram = {
        .phys_start_addr = boot_params->ram_start,
        .size = boot_params->ram_size,
    };
    s_init_state = InitState::Early;
}

void vm_init()
{
    kassert(s_init_state == InitState::Early);
    g_current_address_space = g_kernel_address_space = AddressSpace {
        .ttbr0_page = addr2page(vm_read_current_ttbr0()),
    };

    /**
     * The bootloader had to identity map the physical memory because otherwise
     * it would have died when it enabled the MMU before jumping to the kernel.
     * TODO: Fix the bootloader to not do this
     * 
     * Here we unmap all those pages it mapped, since otherwise they would trip
     * the double-use checks in vm_map
    */
    auto *table = g_kernel_address_space.get_root_table_ptr();
    for (uintptr_t i = 0; i < s_ram.size; i += _1MB) {
        table[lvl1_index(s_ram.phys_start_addr + i)].raw = 0;
    }
    invalidate_tlb();
    
    s_init_state = InitState::Completed;
}

static uintptr_t ioremap_bitmap_alloc(size_t size)
{
    kassert(size % _4KB == 0);
    size_t count = size / _4KB;

    for (size_t i = 0; i < array_size(s_ioremap.used); i++) {
        if (s_ioremap.used[i])
            continue;

        size_t j;
        for (j = i + 1; j < array_size(s_ioremap.used); j++) {
            if (s_ioremap.used[j] || j - i == count)
                break;
        }

        if (j - i == count) {
            memset(s_ioremap.used + i, 1, count);
            return areas::peripherals.start + i * _4KB;
        }
    }

    return 0;
}

static void ioremap_bitmap_free(uintptr_t startaddr, size_t size)
{
    kassert(startaddr >= areas::peripherals.start);
    kassert(startaddr < areas::peripherals.end);
    kassert(startaddr % _4KB == 0);
    kassert(size % _4KB == 0);

    size_t start = (startaddr - areas::peripherals.start) / _4KB;
    size_t count = size / _4KB;
    memset(s_ioremap.used + start, 0, count);
}


/**
 * @brief A simplified version of \ref ioremap that can be used when only vm_init_early was called
*/
static void *ioremap_early(uintptr_t phys_addr, size_t size)
{
    kassert(s_init_state == InitState::Early);

    uintptr_t aligned_phys_addr = round_down<uintptr_t>(phys_addr, _1MB);
    uintptr_t aligned_size = round_up<uintptr_t>(phys_addr + size, _1MB) - aligned_phys_addr;

    uintptr_t start_of_mapping = ioremap_bitmap_alloc(aligned_size);
    if (!start_of_mapping)
        panic_no_print("Failed to allocate early ioremap space");

    auto *table = reinterpret_cast<FirstLevelEntry*>(phys2virt(vm_read_current_ttbr0()));
    for (uintptr_t i = 0; i < aligned_size; i += _1MB) {
        table[lvl1_index(start_of_mapping + i)].section = SectionEntry::make_entry(aligned_phys_addr + i, PageAccessPermissions::PriviledgedOnly);
        invalidate_tlb_entry(start_of_mapping + i);
    }

    return reinterpret_cast<void*>(start_of_mapping + phys_addr % _1MB);
}

void *ioremap(uintptr_t phys_addr, size_t size)
{
    if (s_init_state == InitState::Early)
        return ioremap_early(phys_addr, size);
    
    uintptr_t aligned_phys_addr = round_down<uintptr_t>(phys_addr, _4KB);
    uintptr_t aligned_size = round_up<uintptr_t>(phys_addr + size, _4KB) - aligned_phys_addr;

    uintptr_t start_of_mapping = ioremap_bitmap_alloc(aligned_size);
    if (!start_of_mapping)
        return nullptr;

    MUST(vm_map_mmio(vm_current_address_space(), aligned_phys_addr, start_of_mapping, size));

    return reinterpret_cast<void*>(start_of_mapping + phys_addr % _4KB);
}

void iounmap(void *addr, size_t size)
{
    kassert(s_init_state == InitState::Completed);
    
    uintptr_t virt_addr = reinterpret_cast<uintptr_t>(addr);
    kassert(areas::peripherals.contains(virt_addr));

    uintptr_t start_of_mapping = round_down<uintptr_t>(virt_addr, _4KB);
    uintptr_t aligned_size = round_up<uintptr_t>(virt_addr + size, _4KB) - start_of_mapping;

    ioremap_bitmap_free(start_of_mapping, aligned_size);

    for (uintptr_t i = start_of_mapping; i < start_of_mapping + aligned_size; i += _4KB) {
        uintptr_t previously_mapped_address = 0;
        vm_unmap(vm_current_address_space(), i, previously_mapped_address);
        kassert(previously_mapped_address != 0);
    }
}

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

uintptr_t phys2virt(uintptr_t phys)
{
    return phys - s_ram.phys_start_addr + areas::physical_mem.start;
}

uintptr_t virt2phys(uintptr_t virt)
{
    auto *table = g_current_address_space.get_root_table_ptr();
    auto& lvl1_entry = table[lvl1_index(virt)];
    switch (lvl1_entry.section.identifier) {
    case 0:
        return 0;
    case SECTION_ENTRY_ID:
        return lvl1_entry.section.base_address() | (virt & 0x000fffff);
    case COARSE_PAGE_TABLE_ENTRY_ID: {
        auto *lvl2_table = reinterpret_cast<SecondLevelEntry*>(phys2virt(lvl1_entry.coarse.base_address()));
        auto& lvl2_entry = lvl2_table[lvl2_index(virt)];
        return lvl2_entry.small_page.base_address() | (virt & 0x00000fff);
    }
    default:
        panic("virt2phys: unknown page table entry type, wtf?");
    }
}

Error vm_create_address_space(struct AddressSpace& as)
{
    struct PhysicalPage* as_ttbr0_page;
    TRY(physical_page_alloc(PageOrder::_16KB, as_ttbr0_page));
    
    struct PhysicalPage *as_hack_2nd_level_table_page;
    if (auto e = physical_page_alloc(PageOrder::_1KB, as_hack_2nd_level_table_page); !e.is_success()) {
        physical_page_free(as_ttbr0_page, PageOrder::_16KB);
        return e;
    }

    as.ttbr0_page = as_ttbr0_page;

    FirstLevelEntry* lvl1_table = as.get_root_table_ptr();
    memset(lvl1_table, 0, LVL1_TABLE_SIZE);

    FirstLevelEntry *kernel_lvl1_table = g_kernel_address_space.get_root_table_ptr();
    constexpr uintptr_t KERNEL_START = areas::kernel_area.start;
    for (size_t i = lvl1_index(KERNEL_START); i < LVL1_ENTRIES; i++)
        lvl1_table[i].raw = kernel_lvl1_table[i].raw;

    /**
     * HACK: We need to map the vector table at 0x0 for all AS
     * but we can't just copy kernel_lvl1_table[0].raw directly as
     * it would break the refcount + leak process data in the first MB
     * so we need to refcount++ only the very first page
     * 
     * The proper fix is to use ARM's 'hivec' bit in the SCR register
     * and remap the vector table at 0xffff0000.
     * Meanwhile, this code duplicates only the very first 4KB manually
     * increments the refcount of that first page
     */
    lvl1_table[0].coarse = CoarsePageTableEntry::make_entry(page2addr(as_hack_2nd_level_table_page));
    {
        auto *src_table = reinterpret_cast<SecondLevelEntry*>(phys2virt(kernel_lvl1_table[0].coarse.base_address()));
        struct PhysicalPage *p = addr2page(src_table[0].small_page.base_address());
        p->ref_count++;

        auto *dst_table = reinterpret_cast<SecondLevelEntry*>(phys2virt(lvl1_table[0].coarse.base_address()));
        memset(dst_table, 0, LVL2_TABLE_SIZE);
        dst_table[0].small_page = SmallPageEntry::make_entry(page2addr(p), PageAccessPermissions::PriviledgedOnly);
    }

    return Success;
}

static Error vm_map_page(FirstLevelEntry* root_table, uintptr_t phys_addr, uintptr_t virt_addr, PageAccessPermissions permissions)
{
    auto *kernel_lvl1_table = g_kernel_address_space.get_root_table_ptr();

    auto& lvl1_entry = root_table[lvl1_index(virt_addr)];
    if (lvl1_entry.section.identifier == SECTION_ENTRY_ID)
        panic("vm_map_page: Address %p is already mapped to a section", virt_addr);

    bool lvl2_table_was_just_allocated = false;
    if (lvl1_entry.raw == 0) {
        if (areas::kernel_area.contains(virt_addr)) {
            // This case might happen in the following sequence
            // - This address space is created
            // - While this AS is not loaded, a memory allocation happens which causes a new lvl1 table to be allocated
            // - This AS gets loaded, and a new memory allocation requires to map a new page
            lvl1_entry.raw = kernel_lvl1_table[lvl1_index(virt_addr)].raw;
        }

        if (lvl1_entry.raw == 0) {
            struct PhysicalPage* lvl2_table_page;
            MUST(physical_page_alloc(PageOrder::_1KB, lvl2_table_page));
            lvl1_entry.coarse = CoarsePageTableEntry::make_entry(page2addr(lvl2_table_page));
            lvl2_table_was_just_allocated = true;
        }

        // The kernel area must be mapped the same way in all address spaces.
        // We can afford to have different address spaces not mapped the same way since
        // we can fix them in the page fault handler anyway, but we must always have the
        // kernel_translation_table be the final source of truth for the kernel area.
        if (areas::kernel_area.contains(virt_addr) && root_table != kernel_lvl1_table && lvl2_table_was_just_allocated) {
            kassert(kernel_lvl1_table[lvl1_index(virt_addr)].raw == 0);
            kernel_lvl1_table[lvl1_index(virt_addr)].coarse = lvl1_entry.coarse;
        }
    }

    auto *lvl2_table = reinterpret_cast<SecondLevelEntry*>(phys2virt(lvl1_entry.coarse.base_address()));
    if (lvl2_table_was_just_allocated)
        memset(lvl2_table, 0, LVL2_TABLE_SIZE);

    auto& lvl2_entry = lvl2_table[lvl2_index(virt_addr)];
    if (lvl2_entry.raw != 0)
        panic("vm_map_page: mapping already exists at %p (currenly mapped to %p)", virt_addr, lvl2_entry.small_page.base_address());

    lvl2_entry.small_page = SmallPageEntry::make_entry(phys_addr, permissions);

    invalidate_tlb_entry(virt_addr);
    return Success;
}

static Error vm_map_page(struct AddressSpace& as, uintptr_t phys_addr, uintptr_t virt_addr, PageAccessPermissions permissions)
{
    TRY(vm_map_page(as.get_root_table_ptr(), phys_addr, virt_addr, permissions));
    return Success;
}

Error vm_map(struct AddressSpace& as, struct PhysicalPage* page, uintptr_t virt_addr, PageAccessPermissions permissions)
{
    TRY(vm_map_page(as, page2addr(page), virt_addr, permissions));
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
    auto& lvl1_entry = root_table[lvl1_index(virt_addr)];
    if (lvl1_entry.section.identifier == SECTION_ENTRY_ID)
        panic("vm_unmap_kernel: Address %p is mapped to a section, you can't unmap that!", virt_addr);

    if (lvl1_entry.raw == 0) {
        previously_mapped_physical_address = 0;
        return Success;
    }

    auto *lvl2_table = reinterpret_cast<SecondLevelEntry*>(phys2virt(lvl1_entry.coarse.base_address()));
    auto& lvl2_entry = lvl2_table[lvl2_index(virt_addr)];
    if (lvl2_entry.raw == 0) {
        previously_mapped_physical_address = 0;
        return Success;
    }

    previously_mapped_physical_address = lvl2_entry.small_page.base_address();
    lvl2_entry.raw = 0;
    invalidate_tlb_entry(virt_addr);

    // If the whole level 2 table is empty, and it's not a kernel area address, we can free it
    // Note we don't want to unmap lvl1 tables in the kernel address space because
    // it might cause hard-to-solve inconsistencies with the other address spaces
    if (!areas::kernel_area.contains(virt_addr)) {
        bool whole_lvl2_table_is_empty = true;
        for (size_t i = 0; i < LVL2_ENTRIES; i++) {
            auto& entry = lvl2_table[i];
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
    if (areas::kernel_area.contains(virt_addr))
        return vm_unmap_page(g_kernel_address_space.get_root_table_ptr(), virt_addr, previously_mapped_page);

    TRY(vm_unmap_page(as.get_root_table_ptr(), virt_addr, previously_mapped_page));

    return Success;
}

Error vm_unmap(struct AddressSpace& as, uintptr_t virt_addr, uintptr_t &previously_mapped_physical_address)
{
    TRY(vm_unmap_page(as, virt_addr, previously_mapped_physical_address));
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

void vm_free(struct AddressSpace &as)
{
    auto *lvl1_table = as.get_root_table_ptr();
    if (lvl1_table == nullptr)
        return;
    
    // Note: Do not 'memset' to 0 the pages, their refcount might be > 1 !

    const auto KERNEL_START = areas::kernel_area.start;
    for (size_t i = 0; i < lvl1_index(KERNEL_START); i++) {
        auto &entry = lvl1_table[i]; 
        if (entry.is_empty() || entry.is_section())
            continue;
         
        struct PhysicalPage* p = addr2page(entry.coarse.base_address());
        auto *lvl2_table = reinterpret_cast<SecondLevelEntry*>(phys2virt(entry.coarse.base_address()));
        for (size_t j = 0; j < LVL2_ENTRIES; j++) {
            auto &lvl2_entry = lvl2_table[j];
            if (lvl2_entry.raw == 0)
                continue;
            
            struct PhysicalPage* p = addr2page(lvl2_entry.small_page.base_address());
            MUST(physical_page_free(p, PageOrder::_4KB));
        }

        MUST(physical_page_free(p, PageOrder::_1KB));
    }

    MUST(physical_page_free(as.ttbr0_page, PageOrder::_16KB));
}

Error vm_fork(AddressSpace &as, AddressSpace &out_forked)
{
    Error rc = Success;

    constexpr uintptr_t kernel_start_idx = lvl1_index(areas::kernel_area.start);
    auto *src_lvl1 = as.get_root_table_ptr();
    auto *dst_lvl1 = out_forked.get_root_table_ptr();
    for (size_t i = 0; i < kernel_start_idx; i++) {
        auto &entry = src_lvl1[i];
        if (entry.is_empty())
            continue;
        
        if (entry.is_section())
            panic("forking sections is not implemented!");

        kassert(entry.is_coarse_page());
        
        PhysicalPage *pgtable;
        if (rc = physical_page_alloc(PageOrder::_1KB, pgtable); !rc.is_success())
            goto error;
        memset((void*) phys2virt(page2addr(pgtable)), 0, LVL2_TABLE_SIZE);
        dst_lvl1[i].coarse = CoarsePageTableEntry::make_entry(page2addr(pgtable));
        
        for (size_t j = 0; j < LVL2_ENTRIES; j++) {
            auto &dst_lvl2_entry = reinterpret_cast<SecondLevelEntry*>(phys2virt(page2addr(pgtable)))[j];
            auto &src_lvl2_entry = reinterpret_cast<SecondLevelEntry*>(phys2virt(entry.coarse.base_address()))[j];
            if (src_lvl2_entry.raw == 0)
                continue;
            
            PhysicalPage *page;
            if (rc = physical_page_alloc(PageOrder::_4KB, page); !rc.is_success())
                goto error;
            
            auto *src = reinterpret_cast<void*>(phys2virt(src_lvl2_entry.small_page.base_address()));
            auto *dst = reinterpret_cast<void*>(phys2virt(page2addr(page)));

            memcpy(dst, src, _4KB);
            dst_lvl2_entry.small_page = SmallPageEntry::make_entry(page2addr(page), src_lvl2_entry.small_page.permissions());
        }
    }

    return Success;

error:
    vm_free(out_forked);
    return rc;
}

PageFaultHandlerResult vm_try_fix_page_fault(uintptr_t fault_addr)
{
    uintptr_t phys_ttbr0_addr = vm_read_current_ttbr0();
    if (phys_ttbr0_addr == page2addr(g_kernel_address_space.ttbr0_page))
        return PageFaultHandlerResult::KernelFatal;

    // We don't implement any sort of swap memory, so this is an error in the application for sure
    if (!areas::kernel_area.contains(fault_addr))
        return PageFaultHandlerResult::ProcessFatal;

    auto *lvl1_kernel_table = g_kernel_address_space.get_root_table_ptr();
    if (lvl1_kernel_table[lvl1_index(fault_addr)].raw == 0)
        return PageFaultHandlerResult::KernelFatal;
    
    FirstLevelEntry kernel_lvl1_entry = lvl1_kernel_table[lvl1_index(fault_addr)];
    auto *lvl1_ttbr0_table = reinterpret_cast<FirstLevelEntry*>(phys2virt(phys_ttbr0_addr));
    auto& ttbr0_lvl1_entry = lvl1_ttbr0_table[lvl1_index(fault_addr)];

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
