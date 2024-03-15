#include <kernel/lib/math.h>
#include <kernel/lib/string.h>
#include <kernel/lib/memory.h>
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

    auto must_be_mapped_up_to = round_down<uintptr_t>(new_brk, CHUNK_SIZE);
    if (g_last_mapped_chunk < must_be_mapped_up_to) {
        auto chunks_to_add = (must_be_mapped_up_to - g_last_mapped_chunk) / CHUNK_SIZE;

        for (size_t i = 0; i < chunks_to_add; i++) {
            struct PhysicalPage* page;

            MUST(physical_page_alloc(PageOrder::_4KB, page));
            MUST(vm_map(vm_current_address_space(), page, g_last_mapped_chunk + CHUNK_SIZE, PageAccessPermissions::PriviledgedOnly));

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
    MUST(vm_map(vm_current_address_space(), first_page, g_last_mapped_chunk, PageAccessPermissions::PriviledgedOnly));

    return Success;
}

Error _kmalloc(size_t size, uintptr_t& address)
{
    size = round_up<size_t>(size, 8);

    TRY(sbrk(size, address));

    return Success;
}

Error _kfree(uintptr_t)
{
    return Success;
}

Error krealloc(void*& addr, size_t size)
{
    void* new_addr;
    TRY(kmalloc(size, new_addr));
    // FIXME: This is not the correct way of doing this, but in this dumb implementation
    //        we don't keep the size of the allocated memory anywhere so we don't know
    //        how much to copy. Anyway we know that we can copy at most 'size' bytes, and
    //        due to being a bump allocator we know that the memory is contiguous.
    //        We might be copying some bytes that we shouldn't, but we won't be reading
    //        outside the current brk
    memcpy(new_addr, addr, size);
    if (auto err = kfree(addr); !err.is_success()) {
        kfree(new_addr); // FIXME: How do we handle a double error?
        return err;
    }
    addr = new_addr;
    return Success;
}

}
