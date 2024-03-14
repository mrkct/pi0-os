#pragma once

#include <kernel/error.h>
#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct Storage {
    Error (*read_block)(Storage& storage, uint64_t block_idx, uint8_t* out_buffer);
    Error (*write_block)(Storage& storage, uint64_t block_idx, uint8_t const* buffer);
    Error (*get_block_count)(Storage& storage, uint64_t& out_block_count);

    size_t block_idx_offset;
    void* data;
};

}
