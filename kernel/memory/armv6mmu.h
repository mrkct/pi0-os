#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

static constexpr uint32_t COARSE_PAGE_TABLE_ENTRY_ID = 0b01;
struct CoarsePageTableEntry {
    uint32_t identifier : 2;
    uint32_t sbz : 3;
    uint32_t domain : 4;
    uint32_t impl_defined : 1;
    uint32_t base_addr : 22;
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
};
static_assert(sizeof(SmallPageEntry) == 4, "SmallPageEntry is not 32 bits");

union FirstLevelEntry {
    uint32_t raw;
    CoarsePageTableEntry coarse;
    SectionEntry section;
};
static_assert(sizeof(FirstLevelEntry) == 4, "FirstLevelEntry is not 32 bits");

}
