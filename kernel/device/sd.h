#pragma once

#include <kernel/error.h>
#include <kernel/device/storage.h>


namespace kernel {

struct SDCard {
    bool is_initialized;
    uint32_t ocr, rca;
    uint32_t cid[4];
    uint32_t csd[4];
};

Error sdhc_init();

bool sdhc_contains_card();

Error sdhc_initialize_inserted_card(SDCard&);

Error sd_read_block(SDCard&, uint32_t block_idx, uint32_t block_count, uint8_t* buffer);

Error sd_storage_interface(SDCard&, Storage&, uint32_t block_offset = 0);

}
