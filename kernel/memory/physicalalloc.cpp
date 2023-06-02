#include <kernel/kprintf.h>
#include <kernel/lib/libc/string.h>
#include <kernel/lib/math.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/physicalalloc.h>
#include <kernel/panic.h>

namespace kernel {

static constexpr size_t _16KB = 16 * _1KB;
static constexpr size_t _4KB = 4 * _1KB;

extern "C" uint8_t __kernel_end[];

struct {
    PhysicalPage* data;
    size_t len;
} g_pages;

struct PhysicalPage* g_free_pages_lists[] = {
    [static_cast<size_t>(PageOrder::_1KB)] = nullptr,
    [static_cast<size_t>(PageOrder::_4KB)] = nullptr,
    [static_cast<size_t>(PageOrder::_16KB)] = nullptr,
};

static uintptr_t physical_addr_where_kernel_ends()
{
    auto kernel_end = reinterpret_cast<uintptr_t>(__kernel_end);
    return kernel_end - areas::kernel.start;
}
static PageOrder bigger_order(PageOrder order)
{
    switch (order) {
    case PageOrder::_1KB:
        return PageOrder::_4KB;
    case PageOrder::_4KB:
        return PageOrder::_16KB;
    case PageOrder::_16KB:
        panic("bigger_order: order=_16KB");
        break;
    }

    kassert_not_reached();
}
static PageOrder smaller_order(PageOrder order)
{
    switch (order) {
    case PageOrder::_1KB:
        panic("smaller_order: order=_1KB");
        break;
    case PageOrder::_4KB:
        return PageOrder::_1KB;
    case PageOrder::_16KB:
        return PageOrder::_4KB;
    }

    kassert_not_reached();
}
static struct PhysicalPage* addr2page(uintptr_t addr) { return &g_pages.data[addr / _1KB]; }
uintptr_t page2addr(struct PhysicalPage* page) { return (page - g_pages.data) * _1KB; }
static size_t page2array_index(struct PhysicalPage* page) { return page - g_pages.data; }

static void append_page_to_free_pages_list(struct PhysicalPage* page, PageOrder order)
{
    kassert(page != nullptr);
    kassert(page->next == nullptr);

    page->next = g_free_pages_lists[static_cast<size_t>(order)];
    g_free_pages_lists[static_cast<size_t>(order)] = page;
}

static struct PhysicalPage* pop_page_from_free_pages_list(PageOrder order)
{
    kassert(g_free_pages_lists[static_cast<size_t>(order)] != nullptr);

    auto* page = g_free_pages_lists[static_cast<size_t>(order)];
    g_free_pages_lists[static_cast<size_t>(order)] = page->next;
    page->next = nullptr;

    kassert(page->ref_count == 0);

    return page;
}

static void remove_page_from_free_pages_list(struct PhysicalPage* page, PageOrder order)
{
    kassert(page != nullptr);

    if (g_free_pages_lists[static_cast<size_t>(order)] == nullptr)
        return;

    if (g_free_pages_lists[static_cast<size_t>(order)] == page) {
        g_free_pages_lists[static_cast<size_t>(order)] = page->next;
        page->next = nullptr;
        return;
    }

    auto* prev = g_free_pages_lists[static_cast<size_t>(order)];
    while (prev->next != nullptr) {
        if (prev->next == page) {
            prev->next = page->next;
            page->next = nullptr;
            return;
        }

        prev = prev->next;
    }

    kassert_not_reached();
}

Error physical_page_allocator_init(size_t total_physical_memory_size)
{
    total_physical_memory_size = klib::round_down<size_t>(total_physical_memory_size, _16KB);
    g_pages.len = total_physical_memory_size / _1KB;

    auto start_of_pages_data_addr = physical_addr_where_kernel_ends();
    auto end_of_pages_data_addr = klib::round_up(start_of_pages_data_addr + g_pages.len * sizeof(PhysicalPage), _16KB);
    g_pages.data = reinterpret_cast<PhysicalPage*>(start_of_pages_data_addr);

    memset(g_pages.data, 0, g_pages.len * sizeof(PhysicalPage));

    auto idx_step = _16KB / _1KB;
    auto first_free_page_idx = end_of_pages_data_addr / _1KB;
    auto last_free_page_idx = total_physical_memory_size / _1KB - idx_step;

    for (auto i = last_free_page_idx; i >= first_free_page_idx; i -= idx_step) {
        g_pages.data[i].next = g_free_pages_lists[static_cast<size_t>(PageOrder::_16KB)];
        g_free_pages_lists[static_cast<size_t>(PageOrder::_16KB)] = &g_pages.data[i];
    }

    return Success;
}

static void split_page_in_smaller_chunks(PhysicalPage* page, PageOrder page_order)
{
    kassert(page != nullptr);
    kassert(page->next == nullptr);
    kassert(page_order != PageOrder::_1KB);

    auto chunks_order = smaller_order(page_order);
    auto page_size = order2page_size(chunks_order);

    struct PhysicalPage* buddies[4] = {
        addr2page(page2addr(page) + 0 * page_size),
        addr2page(page2addr(page) + 1 * page_size),
        addr2page(page2addr(page) + 2 * page_size),
        addr2page(page2addr(page) + 3 * page_size),
    };
    for (auto i = 0; i < 4; i++) {
        append_page_to_free_pages_list(buddies[i], chunks_order);
    }
}

static Error _physical_page_alloc(PageOrder order, PhysicalPage*& out_page)
{
    switch (order) {
    case PageOrder::_16KB:
        if (g_free_pages_lists[static_cast<size_t>(PageOrder::_16KB)] == nullptr)
            return OutOfMemory;

        break;
    case PageOrder::_4KB:
        if (g_free_pages_lists[static_cast<size_t>(PageOrder::_4KB)] == nullptr) {
            struct PhysicalPage* bigger_page;
            TRY(_physical_page_alloc(PageOrder::_16KB, bigger_page));
            split_page_in_smaller_chunks(bigger_page, PageOrder::_16KB);
        }

        break;
    case PageOrder::_1KB:
        if (g_free_pages_lists[static_cast<size_t>(PageOrder::_1KB)] == nullptr) {
            struct PhysicalPage* bigger_page;
            TRY(_physical_page_alloc(PageOrder::_4KB, bigger_page));
            split_page_in_smaller_chunks(bigger_page, PageOrder::_4KB);
        }

        break;
    default:
        kassert_not_reached();
    }

    out_page = pop_page_from_free_pages_list(order);

    return Success;
}

Error physical_page_alloc(PageOrder order, PhysicalPage*& out_page)
{
    TRY(_physical_page_alloc(order, out_page));
    out_page->ref_count = 1;

    return Success;
}

static Error _physical_page_free(PhysicalPage* page, PageOrder order)
{
    kassert(page != nullptr);

    if (order == PageOrder::_16KB) {
        append_page_to_free_pages_list(page, PageOrder::_16KB);
        return Success;
    }

    size_t page_index_in_array = page2array_index(page);
    size_t distance_between_buddies = order2page_size(order) / _1KB;
    size_t first_buddy_index_in_array = klib::round_down(page_index_in_array, 4 * distance_between_buddies);

    bool all_buddies_free = true;
    for (auto i = 0; i < 4; i++) {
        auto* this_buddy = &g_pages.data[first_buddy_index_in_array + i * distance_between_buddies];
        if (this_buddy->ref_count != 0) {
            all_buddies_free = false;
            break;
        }
    }

    if (!all_buddies_free) {
        append_page_to_free_pages_list(page, order);
    } else {
        for (int i = 0; i < 4; i++) {
            // FIXME: It's inefficient to iterate over the list all over
            remove_page_from_free_pages_list(&g_pages.data[first_buddy_index_in_array + i * distance_between_buddies], order);
        }
        struct PhysicalPage* bigger_page = &g_pages.data[first_buddy_index_in_array];
        _physical_page_free(bigger_page, bigger_order(order));
    }

    return Success;
}

Error physical_page_free(PhysicalPage* page, PageOrder order)
{
    kassert(page->ref_count >= 1);
    page->ref_count--;

    if (page->ref_count == 0) {
        TRY(_physical_page_free(page, order));
    }

    return Success;
}

}
