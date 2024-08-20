#include <kernel/base.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/physicalalloc.h>
#include <kernel/memory/vm.h>


struct {
    PhysicalPage* data;
    size_t len;
} g_pages;

static uintptr_t s_physical_ram_starting_address;

struct PhysicalPage* g_free_pages_lists[] = {
    [static_cast<size_t>(PageOrder::_1KB)] = nullptr,
    [static_cast<size_t>(PageOrder::_4KB)] = nullptr,
    [static_cast<size_t>(PageOrder::_16KB)] = nullptr,
};

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

struct PhysicalPage* addr2page(uintptr_t addr)
{
    kassert(addr >= s_physical_ram_starting_address);
    auto idx = (addr - s_physical_ram_starting_address) / _1KB;
    kassert(idx < g_pages.len);
    return &g_pages.data[idx];
}

uintptr_t page2addr(struct PhysicalPage* page) { return s_physical_ram_starting_address + (page - g_pages.data) * _1KB; }
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

Error physical_page_allocator_init(BootParams const *boot_params)
{
    s_physical_ram_starting_address = boot_params->ram_start;
    
    size_t total_physical_memory_size = round_down<size_t>(boot_params->ram_size, _16KB);
    g_pages.len = total_physical_memory_size / _1KB;

    // FIXME: This is because we know that the bootloader places the bootmem after
    //        everything else, but we should not depend on that
    uintptr_t first_free_address = boot_params->bootmem_start + boot_params->bootmem_size;

    auto start_of_pages_data_addr = first_free_address;
    auto end_of_pages_data_addr = round_up(start_of_pages_data_addr + g_pages.len * sizeof(PhysicalPage), _16KB);
    g_pages.data = reinterpret_cast<PhysicalPage*>(start_of_pages_data_addr);

    memset(g_pages.data, 0, g_pages.len * sizeof(PhysicalPage));

    auto idx_step = _16KB / _1KB;
    auto first_free_page_idx = (end_of_pages_data_addr - areas::physical_mem.start) / _1KB;
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
        if (g_free_pages_lists[static_cast<size_t>(PageOrder::_16KB)] == nullptr) {
            return OutOfMemory;
        }

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
    size_t first_buddy_index_in_array = round_down(page_index_in_array, 4 * distance_between_buddies);

    bool all_buddies_free = true;
    for (auto i = 0; i < 4; i++) {
        auto* this_buddy = &g_pages.data[first_buddy_index_in_array + i * distance_between_buddies];
        kassert(this_buddy->ref_count >= 0);
        
        /**
         * Note that to check if a page is free we cannot rely on the ref_count, because
         * for example if we are freeing a 4KB page then the ref_count of the page struct
         * at its index might be 0, but that's because the full page has been split in 4 smaller
         * ones and one of them is still in use.
         */
        if (this_buddy->next == nullptr) {
            all_buddies_free = false;
            break;
        }
    }

    if (!all_buddies_free) {
        append_page_to_free_pages_list(page, order);
    } else {
        for (int i = 0; i < 4; i++) {
            auto page_idx = first_buddy_index_in_array + i * distance_between_buddies;

            // Skip because the page we're freeing now was not yet placed in any free list
            if (page_idx == page_index_in_array)
                continue;
            
            // FIXME: It's inefficient to iterate over the list all over
            remove_page_from_free_pages_list(&g_pages.data[page_idx], order);
        }
        struct PhysicalPage* bigger_page = &g_pages.data[first_buddy_index_in_array];
        _physical_page_free(bigger_page, bigger_order(order));
    }

    return Success;
}

Error physical_page_free(PhysicalPage* page, PageOrder order)
{
    kassert(page->ref_count > 0);
    page->ref_count--;

    if (page->ref_count == 0) {
        TRY(_physical_page_free(page, order));
    }

    return Success;
}

void physical_page_print_statistics()
{
    const auto& count_items = [](auto& list) {
        size_t count = 0;
        for (auto* page = list; page != nullptr; page = page->next)
            count++;
        return count;
    };

    kprintf("Physical memory allocator statistics:\n");
    kprintf("  Total 1KB pages: %d\n", g_pages.len);
    kprintf("  Total 4KB pages: %d\n", g_pages.len / 16);
    kprintf("  Total 16KB pages: %d\n", g_pages.len / 64);
    kprintf("  Free 1KB pages: %d\n", count_items(g_free_pages_lists[static_cast<size_t>(PageOrder::_1KB)]));
    kprintf("  Free 4KB pages: %d\n", count_items(g_free_pages_lists[static_cast<size_t>(PageOrder::_4KB)]));
    kprintf("  Free 16KB pages: %d\n", count_items(g_free_pages_lists[static_cast<size_t>(PageOrder::_16KB)]));
}
