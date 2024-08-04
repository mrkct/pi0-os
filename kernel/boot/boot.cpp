#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/arch/arm/armv6mmu.h>
#include "boot.h"
#include "misc.h"


extern "C" uint8_t __bundle_kernel_start[];
extern "C" uint8_t __bundle_kernel_end[];
extern "C" uint8_t __bundle_dtb_start[];
extern "C" uint8_t __bundle_dtb_end[];

#define CONFIG_KERNEL_VIRT_START_ADDRESS        (0xc0000000)
#define CONFIG_PHYSICAL_MEMORY_HOLE_ADDRESS     (0xe0000000)
#define CONFIG_PHYSICAL_MEMORY_HOLE_SIZE        (512 * _1MB)
#define CONFIG_KERNEL_STACK_SIZE                (64 * _1KB)

extern "C" [[noreturn]] void activate_mmu_and_jump_to_kernel(uint32_t ttbr0, uint32_t stack, uint32_t pc);


extern "C" void boot_start(uint32_t, uint32_t, uint32_t, uint32_t load_address)
{
    board_early_console_init();
    early_kprintf("Booting...\n");

    range_t bootmem = board_early_get_bootmem_range();
    bootmem_init(bootmem.start, bootmem.size);
    
    FirstLevelEntry *page_translation_table = (FirstLevelEntry*) bootmem_alloc(_16KB, _16KB);
    memset(page_translation_table, 0, _16KB);
    early_kprintf("Page table: %p\n", page_translation_table);
    
    {
        uintptr_t next_virt = CONFIG_KERNEL_VIRT_START_ADDRESS;
        uintptr_t next_phys = (uintptr_t) __bundle_kernel_start;
        size_t size = round_up<size_t>((uintptr_t) &__bundle_kernel_end - (uintptr_t) &__bundle_kernel_start, _1MB);
        size_t mapped = 0;

        while (mapped < size) {
            const auto lvl1_idx = lvl1_index(next_virt);
            
            if (next_virt % _1MB == 0 && next_phys % _1MB == 0) {
                page_translation_table[lvl1_idx].section = SectionEntry::make_entry(next_phys, PageAccessPermissions::PriviledgedOnly);
                mapped += _1MB;
                continue;
            }

            FirstLevelEntry *lvl1_entry = &page_translation_table[lvl1_idx];
            if (lvl1_entry->is_empty()) {
                void *table = bootmem_alloc(_1KB, _1KB);
                memset(table, 0, _1KB);
                lvl1_entry->coarse = CoarsePageTableEntry::make_entry((uintptr_t) table);
            }
            
            SecondLevelEntry *table = (SecondLevelEntry*) lvl1_entry->coarse.base_address();
            for (auto idx = lvl2_index(next_virt); mapped < size && idx < LVL2_TABLE_SIZE / sizeof(SecondLevelEntry); idx++, mapped += _4KB) {
                EARLY_ASSERT(table[idx].raw == 0);
                table[idx].small_page = SmallPageEntry::make_entry(next_phys, PageAccessPermissions::PriviledgedOnly);
                next_phys += _4KB;
            }
        }
    }

    range_t ram = board_early_get_ram_range();
    if (ram.size > CONFIG_PHYSICAL_MEMORY_HOLE_SIZE)
        ram.size = CONFIG_PHYSICAL_MEMORY_HOLE_SIZE;
    ram.size = round_down<size_t>(ram.size, _1MB);
    early_kprintf("RAM: %p-%p (%uMB)\n", ram.start, ram.start + ram.size, ram.size / _1MB);

    EARLY_ASSERT(ram.start % _1MB == 0);

    for (size_t i = 0; i < ram.size; i += _1MB) {
        auto section = SectionEntry::make_entry(ram.start + i, PageAccessPermissions::PriviledgedOnly);

        auto idx1 = lvl1_index(CONFIG_PHYSICAL_MEMORY_HOLE_ADDRESS + i);
        EARLY_ASSERT(page_translation_table[idx1].raw == 0);
        page_translation_table[idx1].section = section;

        auto idx2 = lvl1_index(ram.start + i);
        EARLY_ASSERT(page_translation_table[idx2].raw == 0);
        page_translation_table[idx2].section = section; 
    }

    /* Setup a larger stack for the kernel, and copy the kernel arguments into it */
    uintptr_t kernel_stack = (uintptr_t) bootmem_alloc(CONFIG_KERNEL_STACK_SIZE, _4KB);
    kernel_stack += CONFIG_KERNEL_STACK_SIZE - 16;

#define REMAP_TO_PHYS_MEMORY_HOLE(addr) ((addr) - ram.start + CONFIG_PHYSICAL_MEMORY_HOLE_ADDRESS)

    BootParams boot_params {
        .ram_start = ram.start,
        .ram_size = ram.size,
    
        .bootmem_start = REMAP_TO_PHYS_MEMORY_HOLE(bootmem.start),
        .bootmem_size = bootmem_allocated(),
    
        .device_tree_start = REMAP_TO_PHYS_MEMORY_HOLE((uintptr_t) __bundle_dtb_start),
        .device_tree_size = (uintptr_t) (__bundle_dtb_end - __bundle_dtb_start),

        .initrd_start = REMAP_TO_PHYS_MEMORY_HOLE(0),
        .initrd_size = 0,
    };
    kernel_stack -= sizeof(BootParams);
    memcpy((void*) kernel_stack, &boot_params, sizeof(BootParams));
    early_kprintf("Kernel stack: %p\n", kernel_stack);


    activate_mmu_and_jump_to_kernel(
        (uint32_t) page_translation_table,
        (uint32_t) kernel_stack,
        (uint32_t) CONFIG_KERNEL_VIRT_START_ADDRESS
    );
}
