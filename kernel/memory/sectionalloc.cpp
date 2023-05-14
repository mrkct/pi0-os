#include <kernel/kprintf.h>
#include <kernel/lib/math.h>
#include <kernel/memory/sectionalloc.h>
#include <kernel/memory/virtualmem.h>

namespace kernel {

extern "C" uintptr_t __heap_start[];

struct Section {
    enum class Status {
        Free,
        InUse,
        Reserved
    };
    Status status;
    Section* next_free;
};

static struct {
    Section* data;
    size_t length;
} g_sections;
static Section* g_free_sections_list = nullptr;

void section_allocator_init()
{
    // TODO: Get the VC memory from the firmware
    constexpr uint64_t VIDEOCORE_RESERVED_MEMORY = 32 * _1MB;
    constexpr uint64_t MEMORY_SIZE = 512 * _1MB - VIDEOCORE_RESERVED_MEMORY;
    g_sections.length = MEMORY_SIZE / SECTION_SIZE;
    g_sections.data = reinterpret_cast<Section*>(__heap_start);

    // Set all pages below the heap, and the g_pages array itself, as RESERVED
    uintptr_t first_available_section = klib::round_up(
                                            virt2phys(reinterpret_cast<uintptr_t>(&g_sections.data[g_sections.length])), SECTION_SIZE)
        / SECTION_SIZE;
    for (size_t i = 0; i < first_available_section; i++) {
        g_sections.data[i].status = Section::Status::Reserved;
        g_sections.data[i].next_free = nullptr;
    }

    // And all the rest as FREE
    for (size_t i = first_available_section; i < g_sections.length; i++) {
        g_sections.data[i].status = Section::Status::InUse;
        g_sections.data[i].next_free = g_free_sections_list;
        g_free_sections_list = &g_sections.data[i];
    }
}

Error section_alloc(uintptr_t& section)
{
    if (g_free_sections_list == nullptr)
        return OutOfMemory;

    auto* free_section = g_free_sections_list;
    g_free_sections_list = free_section->next_free;
    free_section->status = Section::Status::InUse;
    section = SECTION_SIZE * (free_section - g_sections.data);

    return Success;
}

Error section_free(uintptr_t section)
{
    if (section % SECTION_SIZE != 0 || section >= g_sections.length * SECTION_SIZE)
        return BadParameters;

    auto* section_to_free = &g_sections.data[section / SECTION_SIZE];
    if (section_to_free->status != Section::Status::InUse)
        return BadParameters;

    section_to_free->status = Section::Status::Free;
    section_to_free->next_free = g_free_sections_list;
    g_free_sections_list = section_to_free;

    return Success;
}

}
