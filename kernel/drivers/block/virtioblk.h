#pragma once

#include <kernel/drivers/device.h>
#include "virtio_queue.h"


struct SplitVirtQueue {
    size_t size;
    PhysicalPage *alloc_page;

    volatile struct virtq_desc *desc_table;
    volatile struct virtq_avail *avail;
    volatile struct virtq_used *used;
};

class VirtioBlockDevice: public BlockDevice
{
public:
    struct Config {
        uintptr_t address;
        uint32_t irq;
    };

    static bool probe(uintptr_t address);

    struct RegisterMap {
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
    static_assert(offsetof(RegisterMap, DriverFeatures) == 0x20);
    static_assert(offsetof(RegisterMap, QueueSel) == 0x30);
    static_assert(offsetof(RegisterMap, QueueReady) == 0x44);
    static_assert(offsetof(RegisterMap, QueueNotify) == 0x50);
    static_assert(offsetof(RegisterMap, InterruptStatus) == 0x60);
    static_assert(offsetof(RegisterMap, QueueDescLow) == 0x80);
    static_assert(offsetof(RegisterMap, QueueDeviceLow) == 0xa0);
    static_assert(offsetof(RegisterMap, ConfigGeneration) == 0xfc);
    static_assert(offsetof(RegisterMap, block) == 0x100);

    VirtioBlockDevice(Config const *config)
        : BlockDevice("virtioblk"), m_config(*config) 
    {}

    virtual int32_t init() override;
    virtual int32_t shutdown() override { return 0; }

    virtual int64_t read(int64_t offset, uint8_t *buffer, size_t size) override;
    virtual int64_t write(int64_t offset, const uint8_t *buffer, size_t size) override;
    virtual int32_t ioctl(uint32_t request, void *argp) override;
    virtual uint64_t size() const override { return m_capacity; }

private:
    int32_t block_device_init();
    int32_t init_virtqueue(uint32_t index, uint32_t max_size);

    Config m_config;
    RegisterMap volatile *r;
    bool m_ready { false };
    SplitVirtQueue *m_vqueue;

    bool m_readonly;
    uint64_t m_capacity;
};
