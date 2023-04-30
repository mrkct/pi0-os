#include <kernel/kprintf.h>
#include <kernel/lib/math.h>
#include <kernel/memory/pagealloc.h>

namespace kernel {

static constexpr size_t PAGE_SIZE = 4096;

extern "C" uintptr_t __heap_start[];

struct PageFlags {
    enum class Status {
        Free,
        InUse,
        Reserved,
        Peripheral,
    };
    Status status;
};

struct Page {
    PageFlags flags;
    Page* next_free;
};

static struct {
    Page* data;
    size_t length;
} g_pages;
static Page* g_free_pages_list = nullptr;

void page_allocator_init()
{
    const uint64_t MEMORY_SIZE = 512 * 1024 * 1024;
    g_pages.length = MEMORY_SIZE / PAGE_SIZE;
    g_pages.data = reinterpret_cast<Page*>(__heap_start);

    // Set all pages below the heap, and the g_pages array itself, as RESERVED
    uintptr_t first_available_page = klib::round_up(reinterpret_cast<uintptr_t>(&g_pages.data[g_pages.length]), PAGE_SIZE);
    for (size_t i = 0; i < first_available_page / PAGE_SIZE; i++) {
        g_pages.data[i].flags.status = PageFlags::Status::Reserved;
        g_pages.data[i].next_free = nullptr;
    }

    // And all the rest as FREE
    for (size_t i = first_available_page / PAGE_SIZE; i < g_pages.length; i++) {
        g_pages.data[i].flags.status = PageFlags::Status::Free;
        g_pages.data[i].next_free = g_free_pages_list;
        g_free_pages_list = &g_pages.data[i];
    }

    // TODO: Mark all pages that are used by peripherals as PERIPHERAL
}

Error page_alloc(uintptr_t& page)
{
    if (g_free_pages_list == nullptr)
        return OutOfMemory;

    Page* free_page = g_free_pages_list;
    g_free_pages_list = free_page->next_free;
    free_page->flags.status = PageFlags::Status::InUse;
    page = PAGE_SIZE * (free_page - g_pages.data);

    return Success;
}

Error page_free(uintptr_t page)
{
    if (page % PAGE_SIZE != 0 || page >= g_pages.length * PAGE_SIZE)
        return BadParameters;

    Page* page_to_free = &g_pages.data[page / PAGE_SIZE];
    if (page_to_free->flags.status != PageFlags::Status::InUse)
        return BadParameters;

    page_to_free->flags.status = PageFlags::Status::Free;
    page_to_free->next_free = g_free_pages_list;
    g_free_pages_list = page_to_free;

    return Success;
}

}
