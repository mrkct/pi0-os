#include <kernel/lib/math.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/sectionalloc.h>
#include <kernel/memory/virtualmem.h>

namespace kernel {

static uintptr_t g_last_mapped_chunk = areas::heap.start;
static uintptr_t g_brk = areas::heap.start;

static Error brk(uintptr_t new_brk)
{
    if (new_brk < areas::heap.start || new_brk > areas::heap.end)
        return BadParameters;

    auto must_be_mapped_up_to = klib::round_down<uintptr_t>(new_brk, SECTION_SIZE);
    if (g_last_mapped_chunk < must_be_mapped_up_to) {
        auto sections_to_add = (must_be_mapped_up_to - g_last_mapped_chunk) / SECTION_SIZE;

        for (size_t i = 0; i < sections_to_add; i++) {
            uintptr_t phys_section;

            TRY(section_alloc(phys_section));
            TRY(mmu_map_section(phys_section, g_last_mapped_chunk, VirtualSectionType::KernelHeap));

            g_last_mapped_chunk += SECTION_SIZE;
        }
    } else if (must_be_mapped_up_to < g_last_mapped_chunk) {
        auto pages_to_remove = (g_last_mapped_chunk - must_be_mapped_up_to) / SECTION_SIZE;

        for (size_t i = 0; i < pages_to_remove; i++) {
            auto section_to_free = virt2phys(g_last_mapped_chunk);

            MUST(mmu_unmap_section(g_last_mapped_chunk));
            MUST(section_free(section_to_free));

            g_last_mapped_chunk -= SECTION_SIZE;
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

    uintptr_t phys_section;

    TRY(section_alloc(phys_section));
    TRY(mmu_map_section(phys_section, g_last_mapped_chunk, VirtualSectionType::KernelHeap));

    *reinterpret_cast<uint32_t*>(g_last_mapped_chunk) = 0x1234;

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
