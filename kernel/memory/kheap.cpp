#include <kernel/lib/math.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/physicalalloc.h>
#include <kernel/memory/vm.h>

namespace kernel {

static uintptr_t g_last_mapped_chunk = areas::heap.start;
static uintptr_t g_brk = areas::heap.start;

static size_t CHUNK_SIZE = 4 * _1KB;

static Error brk(uintptr_t new_brk)
{
    if (new_brk < areas::heap.start || new_brk > areas::heap.end)
        return BadParameters;

    auto must_be_mapped_up_to = klib::round_down<uintptr_t>(new_brk, CHUNK_SIZE);
    if (g_last_mapped_chunk < must_be_mapped_up_to) {
        auto chunks_to_add = (must_be_mapped_up_to - g_last_mapped_chunk) / CHUNK_SIZE;

        for (size_t i = 0; i < chunks_to_add; i++) {
            struct PhysicalPage* page;

            TRY(physical_page_alloc(PageOrder::_4KB, page));
            TRY(vm_map(vm_current_address_space(), page, g_last_mapped_chunk + (i + 1) * CHUNK_SIZE));

            g_last_mapped_chunk += CHUNK_SIZE;
        }
    } else if (must_be_mapped_up_to < g_last_mapped_chunk) {
        auto chunks_to_remove = (g_last_mapped_chunk - must_be_mapped_up_to) / CHUNK_SIZE;

        for (size_t i = 0; i < chunks_to_remove; i++) {
            struct PhysicalPage* to_free;

            MUST(vm_unmap(vm_current_address_space(), g_last_mapped_chunk, to_free));
            MUST(physical_page_free(to_free, PageOrder::_4KB));

            g_last_mapped_chunk -= CHUNK_SIZE;
        }
    }

    g_brk = new_brk;

    return Success;
}

static Error sbrk(size_t size, uintptr_t& address)
{
    if (g_brk > areas::heap.end - size)
        return OutOfMemory;

    auto old_brk = g_brk;
    TRY(brk(g_brk + size));
    address = old_brk;

    return Success;
}

Error kheap_init()
{
    struct PhysicalPage* first_page;

    TRY(physical_page_alloc(PageOrder::_4KB, first_page));
    TRY(vm_map(vm_current_address_space(), first_page, g_last_mapped_chunk));

    return Success;
}

Error _kmalloc(size_t size, uintptr_t& address)
{
    size = klib::round_up<size_t>(size, 8);

    TRY(sbrk(size, address));

    return Success;
}

Error _kfree(uintptr_t)
{
    return Success;
}

}
