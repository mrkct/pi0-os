#pragma once

#include <kernel/error.h>

namespace kernel {

void page_allocator_init();

Error page_alloc(intptr_t& page);

Error page_free(intptr_t page);

}
