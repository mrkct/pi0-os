#pragma once

#include <stdint.h>


typedef struct BootParams {
    uint32_t ram_start;
    uint32_t ram_size;
    
    uint32_t bootmem_start;
    uint32_t bootmem_size;
    
    uint32_t device_tree_start;
    uint32_t device_tree_size;

    uint32_t initrd_start;
    uint32_t initrd_size;
} BootParams;
