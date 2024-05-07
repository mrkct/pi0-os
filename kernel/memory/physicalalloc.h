#pragma once

#include <kernel/error.h>
#include <kernel/sizes.h>
#include <stdint.h>

namespace kernel {

struct PhysicalPage {
    int32_t ref_count;
    struct PhysicalPage* next;
};

enum class PageOrder {
    _1KB,
    _4KB,
    _16KB
};

static inline size_t order2page_size(PageOrder order)
{
    switch (order) {
    case PageOrder::_16KB:
        return _16KB;
    case PageOrder::_4KB:
        return _4KB;
    case PageOrder::_1KB:
        return _1KB;
    }

    kassert(false);
}

uintptr_t page2addr(struct PhysicalPage* page);

struct PhysicalPage* addr2page(uintptr_t addr);

Error physical_page_allocator_init(size_t total_physical_memory_size);

Error physical_page_allocator_set_memory_range_as_reserved(uintptr_t start, uintptr_t end);

Error physical_page_alloc(PageOrder, PhysicalPage*&);

Error physical_page_free(PhysicalPage*, PageOrder);

void physical_page_print_statistics();

}
