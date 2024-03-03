#pragma once

#include <kernel/sizes.h>
#include <stddef.h>
#include <stdint.h>

namespace kernel {

static constexpr size_t lvl1_index(uintptr_t virt) { return virt >> 20; }
static constexpr size_t lvl2_index(uintptr_t virt) { return (virt >> 12) & 0xff; }

static constexpr size_t LVL1_TABLE_SIZE = 16 * _1KB;
static constexpr size_t LVL2_TABLE_SIZE = _1KB;

static inline void invalidate_tlb()
{
    // "Invalidate entire unified TLB or both instruction and data TLBs"
    asm volatile("mcr p15, 0, %0, c8, c7, 0" ::"r"(0));
}

static inline void invalidate_tlb_entry(uintptr_t virt_addr)
{
    asm volatile("mcr p15, 0, %0, c8, c7, 1" ::"r"(virt_addr));
}

/**
 * \brief Read the 'Data Fault Status' register
 */
static inline uint32_t read_dfsr()
{
    uint32_t status;
    asm volatile("mrc p15, 0, %0, c5, c0, 0" :"=r"(status));
    return status;
}

/**
 * \brief Extract the "Fault Status" value from a "Data Fault Status Register"
*/
static inline uint32_t dfsr_fault_status(uint32_t dfsr)
{
    return (dfsr & 0xf) | ((dfsr >> 10 & 1) << 4);
}

const char *dfsr_status_to_string(uint32_t dfsr_status);

static inline uintptr_t read_fault_address_register()
{
    uintptr_t addr;
    asm volatile("mrc p15, 0, %0, c6, c0, 0" :"=r"(addr));
    return addr;
}

enum class PageAccessPermissions : uint8_t {
    Unmapped = 0b00,
    PriviledgedOnly = 0b01,
    UserReadOnly = 0b10,
    UserFullAccess = 0b11
};

static constexpr uint32_t COARSE_PAGE_TABLE_ENTRY_ID = 0b01;
struct CoarsePageTableEntry {
    uint32_t identifier : 2;
    uint32_t sbz : 3;
    uint32_t domain : 4;
    uint32_t impl_defined : 1;
    uint32_t base_addr : 22;

    uintptr_t base_address() const { return (base_addr << 10) & ~0x3ff; }

    static CoarsePageTableEntry make_entry(uintptr_t address)
    {
        return {
            .identifier = COARSE_PAGE_TABLE_ENTRY_ID,
            .sbz = 0,
            .domain = 0,
            .impl_defined = 0,
            .base_addr = ((uint32_t)address >> 10) & 0x3fffff,
        };
    }
};
static_assert(sizeof(CoarsePageTableEntry) == 4, "CoarsePageTableEntry is not 32 bits");

static constexpr uint32_t SECTION_ENTRY_ID = 0b10;
struct SectionEntry {
    uint32_t identifier : 2;
    uint32_t bufferable_writes : 1;
    uint32_t cachable : 1;
    uint32_t sbz : 1;
    uint32_t domain : 4;
    uint32_t impl_defined : 1;
    uint32_t access_permission : 2;
    uint32_t tex : 3;
    uint32_t sbz2 : 5;
    uint32_t base_addr : 12;

    uintptr_t base_address() const { return base_addr << 20; }

    static SectionEntry make_entry(uintptr_t addr, PageAccessPermissions permissions)
    {
        return {
            .identifier = SECTION_ENTRY_ID,
            .bufferable_writes = 0,
            .cachable = 0,
            .sbz = 0,
            .domain = 0,
            .impl_defined = 0,
            .access_permission = static_cast<uint8_t>(permissions),
            .tex = 0b000,
            .sbz2 = 0,
            .base_addr = ((uint32_t)addr >> 20) & 0xfff,
        };
    }
};
static_assert(sizeof(SectionEntry) == 4, "SectionEntry is not 32 bits");

static constexpr uint32_t SMALL_PAGE_ENTRY_ID = 0b10;
struct SmallPageEntry {
    uint32_t identifier : 2;
    uint32_t bufferable_writes : 1;
    uint32_t cachable : 1;
    uint32_t ap0 : 2;
    uint32_t ap1 : 2;
    uint32_t ap2 : 2;
    uint32_t ap3 : 2;
    uint32_t address : 20;

    uintptr_t base_address() const { return address << 12; }

    static SmallPageEntry make_entry(uintptr_t address, PageAccessPermissions permissions)
    {
        return {
            .identifier = SMALL_PAGE_ENTRY_ID,
            .bufferable_writes = 0,
            .cachable = 0,
            .ap0 = static_cast<uint8_t>(permissions),
            .ap1 = static_cast<uint8_t>(permissions),
            .ap2 = static_cast<uint8_t>(permissions),
            .ap3 = static_cast<uint8_t>(permissions),
            .address = ((uint32_t)address >> 12) & 0xfffff,
        };
    }
};
static_assert(sizeof(SmallPageEntry) == 4, "SmallPageEntry is not 32 bits");

union FirstLevelEntry {
    uint32_t raw;
    CoarsePageTableEntry coarse;
    SectionEntry section;
};
static_assert(sizeof(FirstLevelEntry) == 4, "FirstLevelEntry is not 32 bits");

union SecondLevelEntry {
    uint32_t raw;
    SmallPageEntry small_page;
};
static_assert(sizeof(SecondLevelEntry) == 4, "SecondLevelEntry is not 32 bits");

}
