#include <kernel/base.h>
#include <stdlib.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/physicalalloc.h>
#include <kernel/memory/vm.h>


// #define LOG_ENABLED
#define LOG_TAG "BRK"
#include <kernel/log.h>

static uintptr_t g_last_mapped_chunk = areas::kernel_heap.start;
static uintptr_t g_brk = areas::kernel_heap.start;

static size_t CHUNK_SIZE = 4 * _1KB;

static Error brk(uintptr_t new_brk)
{
    LOGD("Moving brk from %p to brk: %p", g_brk, new_brk);
    if (new_brk < areas::kernel_heap.start || new_brk > areas::kernel_heap.end)
        return BadParameters;

    auto must_be_mapped_up_to = round_down<uintptr_t>(new_brk, CHUNK_SIZE);
    if (g_last_mapped_chunk < must_be_mapped_up_to) {
        auto chunks_to_add = (must_be_mapped_up_to - g_last_mapped_chunk) / CHUNK_SIZE;

        for (size_t i = 0; i < chunks_to_add; i++) {
            struct PhysicalPage* page;

            MUST(physical_page_alloc(PageOrder::_4KB, page));
            LOGD("Mapping page %p at %p", page2addr(page), g_last_mapped_chunk + CHUNK_SIZE);
            MUST(vm_map(vm_current_address_space(), page, g_last_mapped_chunk + CHUNK_SIZE, PageAccessPermissions::PriviledgedOnly));

            g_last_mapped_chunk += CHUNK_SIZE;
        }
    } else if (must_be_mapped_up_to < g_last_mapped_chunk) {
        auto chunks_to_remove = (g_last_mapped_chunk - must_be_mapped_up_to) / CHUNK_SIZE;

        for (size_t i = 0; i < chunks_to_remove; i++) {
            struct PhysicalPage* to_free;
            uintptr_t previously_mapped_physical_address = 0;

            MUST(vm_unmap(vm_current_address_space(), g_last_mapped_chunk, previously_mapped_physical_address));
            to_free = addr2page(previously_mapped_physical_address);
            MUST(physical_page_free(to_free, PageOrder::_4KB));
            LOGD("Unmapping page %p from %p", previously_mapped_physical_address, g_last_mapped_chunk);

            g_last_mapped_chunk -= CHUNK_SIZE;
        }
    }

    g_brk = new_brk;

    return Success;
}

static Error sbrk(size_t size, uintptr_t& address)
{
    if (g_brk > areas::kernel_heap.end - size)
        return OutOfMemory;

    auto old_brk = g_brk;
    TRY(brk(g_brk + size));
    address = old_brk;

    return Success;
}

extern "C" void* _sbrk(int incr)
{
    uintptr_t addr;
    auto err = sbrk(incr, addr);
    if (!err.is_success()) {
        return nullptr;
    }

    return reinterpret_cast<void*>(addr);
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
    void *addr = malloc(size);
    if (addr == nullptr)
        return OutOfMemory;

    address = reinterpret_cast<uintptr_t>(addr);
    return Success;
}

Error _kfree(uintptr_t address)
{
    free(reinterpret_cast<void*>(address));
    return Success;
}

Error krealloc(void*& addr, size_t size)
{
    void *a = realloc(addr, size);
    if (a == nullptr)
        return OutOfMemory;
    addr = a;
    return Success;
}
