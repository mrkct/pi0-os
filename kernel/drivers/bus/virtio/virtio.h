#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel/memory/physicalalloc.h>
#include "virtio_queue.h"


enum class VirtioDeviceID: uint32_t {
    Invalid,
    BlockDevice = 2,
    Unsupported = 0xffffffff
};

struct SplitVirtQueue {
    size_t idx;
    size_t size;
    PhysicalPage *alloc_page;
    uint16_t first_free_desc_idx;
    uint16_t last_seen_used_idx;

    volatile struct virtq_desc *desc_table;
    volatile struct virtq_avail *avail;
    volatile struct virtq_used *used;
};

struct VirtioRegisterMap {
    uint32_t MagicValue;
    uint32_t Version;
    uint32_t DeviceID;
    uint32_t VendorID;
        
    uint32_t DeviceFeatures;
    uint32_t DeviceFeaturesSel;
    uint32_t reserved1[2];
        
    uint32_t DriverFeatures;
    uint32_t DriverFeaturesSel;
    uint32_t reserved2[2];

    uint32_t QueueSel;
    uint32_t QueueNumMax;
    uint32_t QueueNum;
    uint32_t reserved3[2];

    uint32_t QueueReady;
    uint32_t reserved4[2];
    uint32_t QueueNotify;
    uint32_t reserved5[3];
    uint32_t InterruptStatus;
    uint32_t InterruptAck;
    uint32_t reserved6[2];

    uint32_t Status;
    uint32_t reserved7[3];

    uint32_t QueueDescLow;
    uint32_t QueueDescHigh;
    uint32_t reserved8[2];
    uint32_t QueueDriverLow;
    uint32_t QueueDriverHigh;
    uint32_t reserved9[2];
    uint32_t QueueDeviceLow;
    uint32_t QueueDeviceHigh;
    uint32_t reserved10[21];
    uint32_t ConfigGeneration;
    union {
        struct {
            uint64_t capacity;
            uint32_t size_max;
            uint32_t seg_max;
            struct virtio_blk_geometry {
                uint16_t cylinders;
                uint8_t heads;
                uint8_t sectors;
            } geometry;
            uint32_t blk_size;
            struct virtio_blk_topology {
                uint8_t physical_block_exp;
                uint8_t alignment_offset;
                uint16_t min_io_size;
                uint32_t opt_io_size;
            } topology;
            uint8_t writeback;
            uint8_t unused[3];
            uint32_t max_discard_sectors;
            uint32_t max_discard_seg;
            uint32_t discard_sector_alignment;
            uint32_t max_write_zeroes_sectors;
            uint32_t max_write_zeroes_seg;
            uint8_t write_zeroes_may_unmap;
            uint8_t unused2[3];
        } block;
    };
} __attribute__((packed));
static_assert(offsetof(VirtioRegisterMap, DriverFeatures) == 0x20);
static_assert(offsetof(VirtioRegisterMap, QueueSel) == 0x30);
static_assert(offsetof(VirtioRegisterMap, QueueReady) == 0x44);
static_assert(offsetof(VirtioRegisterMap, QueueNotify) == 0x50);
static_assert(offsetof(VirtioRegisterMap, InterruptStatus) == 0x60);
static_assert(offsetof(VirtioRegisterMap, QueueDescLow) == 0x80);
static_assert(offsetof(VirtioRegisterMap, QueueDeviceLow) == 0xa0);
static_assert(offsetof(VirtioRegisterMap, ConfigGeneration) == 0xfc);
static_assert(offsetof(VirtioRegisterMap, block) == 0x100);

VirtioDeviceID virtio_util_probe(uintptr_t address);

int32_t virtio_util_do_generic_unit_step(
    VirtioRegisterMap volatile *r,
    VirtioDeviceID expected_device_class,
    uint32_t supported_features
);

int32_t virtio_util_complete_init_step(VirtioRegisterMap volatile *r);

int32_t virtio_util_init_failure(VirtioRegisterMap volatile *r);

int32_t virtio_util_setup_virtq(
    VirtioRegisterMap volatile *r,
    uint32_t index,
    uint32_t size,
    SplitVirtQueue **out_q
);

int virtio_virtq_alloc_desc(SplitVirtQueue *q);

void virtio_virtq_free_desc(SplitVirtQueue *q, int desc_idx);

void virtio_virtq_enqueue_desc(
    VirtioRegisterMap volatile *r,
    SplitVirtQueue *q, int desc_idx
);
