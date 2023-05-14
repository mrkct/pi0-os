#pragma once

#include <kernel/error.h>
#include <kernel/sizes.h>

namespace kernel {

static constexpr size_t SECTION_SIZE = _1MB;

void section_allocator_init();

Error section_alloc(uintptr_t& section);

Error section_free(uintptr_t section);

}
