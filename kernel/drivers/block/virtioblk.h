#pragma once

#include <kernel/drivers/device.h>
#include <kernel/locking/spinlock.h>
#include <kernel/drivers/bus/virtio/virtio.h>


struct VirtioBlockRequest {
    INTRUSIVE_LINKED_LIST_HEADER(VirtioBlockRequest);

    SplitVirtQueue *queue;
    uint16_t descriptor_idx[3];
    Spinlock completed;

    struct {
        virtio_blk_req_header header;
        uint8_t *data;
        virtio_blk_req_footer footer;
    } req;
};

class VirtioBlockDevice: public SimpleBlockDevice
{
public:
    struct Config {
        uintptr_t address;
        uint32_t irq;
    };

    static bool probe(uintptr_t address);

    VirtioBlockDevice(Config const *config)
        : SimpleBlockDevice(512, "virtioblk"), m_config(*config) 
    {}

    virtual int32_t init() override;
    virtual int32_t shutdown() override { return 0; }
    virtual int32_t ioctl(uint32_t request, void *argp) override;
    virtual uint64_t size() const override { return m_capacity; }

protected:
    virtual int64_t read_sector(int64_t sector_idx, uint8_t *buffer) override;
    virtual int64_t write_sector(int64_t sector_idx, uint8_t const *buffer) override;
    virtual bool is_read_only() const override { return m_readonly; }

private:
    

    int32_t block_device_init();
    int32_t init_virtqueue(uint32_t index, uint32_t max_size);

    int enqueue_block_request(uint32_t type, uint32_t sector, uint8_t *buffer, VirtioBlockRequest *req);
    void process_used_buffer(SplitVirtQueue *q, uint32_t idx);
    void handle_irq();
    void cleanup_block_request(VirtioBlockRequest *req);

    Config m_config;
    VirtioRegisterMap volatile *r;
    bool m_ready { false };
    SplitVirtQueue *m_vqueue;

    IntrusiveLinkedList<VirtioBlockRequest> m_requests;

    bool m_readonly;
    uint64_t m_capacity;
};
