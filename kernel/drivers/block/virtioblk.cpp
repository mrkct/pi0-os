#include "virtioblk.h"
#include <kernel/lib/arrayutils.h>
#include <kernel/locking/irqlock.h>

// #define LOG_ENABLED
#define LOG_TAG "VBLK"
#include <kernel/log.h>


static constexpr uint32_t VIRTQ_SIZE = 64;

enum VirtioBlockDeviceFeatures: uint32_t {
    ReadOnly = 1 << 5,
};

static constexpr uint32_t VIRTIO_SUPPORTED_FEATURES = 0;
static constexpr uint32_t BLOCK_DEVICE_SUPPORTED_FEATURES = VirtioBlockDeviceFeatures::ReadOnly;


int32_t VirtioBlockDevice::init()
{
    int32_t rc = 0;

    r = static_cast<VirtioRegisterMap volatile*>(ioremap(m_config.address, sizeof(VirtioRegisterMap)));
    if (!r) {
        rc = -ENOMEM;
        goto failed;
    }
    LOGD("Registers are mapped at 0x%p", r);

    rc = virtio_util_do_generic_init_step(r, VirtioDeviceID::BlockDevice, BLOCK_DEVICE_SUPPORTED_FEATURES);
    if (rc != 0) {
        LOGE("Failed virtio generic init step: %" PRId32);
        goto failed;
    }
    
    // 7. Perform device-specific setup, including discovery of virtqueues for the device,
    //    optional per-bus setup, reading and possibly writing the device’s virtio
    //    configuration space, and population of virtqueues.
    rc = block_device_init();
    if (rc) {
        LOGE("Failed device specific init step: %" PRId32);
        goto failed;
    }
    
    rc = virtio_util_complete_init_step(r);
    if (rc != 0) {
        LOGE("Failed virtio init completion step: %" PRId32);
        goto failed;
    }
    
    irq_install(m_config.irq, [](InterruptFrame*, void *arg) {
        static_cast<VirtioBlockDevice*>(arg)->handle_irq();
    }, this);
    irq_mask(m_config.irq, false);

    return 0;

failed:
    virtio_util_init_failure(r);
    iounmap((void*) r, sizeof(VirtioRegisterMap));
    r = nullptr;
    return rc;
}

int32_t VirtioBlockDevice::block_device_init()
{
    int32_t rc;
    
    rc = virtio_util_setup_virtq(r, 0, VIRTQ_SIZE, &m_vqueue);
    if (rc)
        return rc;

    // 5.2.5 Device Initialization

    // 1. The device size can be read from capacity.
    m_capacity = ioread32(&r->block.capacity);
    m_capacity *= 512;

    // NOTE: All the steps after are related to optional features,
    //       and therefore skippable
    iowrite32(&r->DeviceFeaturesSel, 0);
    uint32_t features = ioread32(&r->DeviceFeatures);

    // 3. If the VIRTIO_BLK_F_RO feature is set by the device, any write
    //    requests will fail.
    if (features & VirtioBlockDeviceFeatures::ReadOnly) {
        LOGI("Disk is read-only");
        m_readonly = true;
    }

    return 0;
}

int VirtioBlockDevice::enqueue_block_request(uint32_t type, uint32_t sector, uint8_t *buffer, VirtioBlockRequest *req)
{
    SplitVirtQueue *q = m_vqueue;
    int rc = 0;
    int header_idx = 0;
    int body_idx = 0;
    int status_idx = 0;

    // The output buffer must not cross a page boundary, see the comment in 'read_sector' for why
    {
        uintptr_t pageidx1 = (uintptr_t) buffer >> 12;
        uintptr_t pageidx2 = ((uintptr_t) buffer + 512 - 1) >> 12;
        kassert(pageidx1 == pageidx2);
    }

    header_idx = virtio_virtq_alloc_desc(q);
    body_idx = virtio_virtq_alloc_desc(q);
    status_idx = virtio_virtq_alloc_desc(q);
    if (header_idx < 0 || body_idx < 0 || status_idx < 0) {
        LOGE("Failed to allocate virtqueue descriptors");
        rc = -ENOMEM;
        goto failed;
    }

    LOGI("Enqueuing block request: type=%d, sector=%d, desc=(%d, %d, %d)", type, sector, header_idx, body_idx, status_idx);
    *req = VirtioBlockRequest {
        .prev = nullptr,
        .next = nullptr,
        
        .queue = m_vqueue,
        .descriptor_idx = {(uint16_t) header_idx, (uint16_t) body_idx, (uint16_t) status_idx},
        .completed = SPINLOCK_START,
        .req = {
            .header = {
                .type = type,
                .reserved = 0,
                .sector = sector,
            },
            .data = buffer,
            .footer = {
                .status = 0,
            }
        }
    };
    spinlock_take(req->completed);
    {
        auto lock = irq_lock();
        m_requests.add(req);
        release(lock);
    }

    q->desc_table[header_idx].addr = virt2phys((uintptr_t) &(req->req.header));
    q->desc_table[header_idx].len = 16;
    q->desc_table[header_idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc_table[header_idx].next = (le16) body_idx;

    q->desc_table[body_idx].addr = virt2phys((uintptr_t) req->req.data);
    q->desc_table[body_idx].len = 512;
    q->desc_table[body_idx].flags = VIRTQ_DESC_F_NEXT | (type == VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0);
    q->desc_table[body_idx].next = (le16) status_idx;

    q->desc_table[status_idx].addr = virt2phys((uintptr_t) &(req->req.footer));
    q->desc_table[status_idx].len = 1;
    q->desc_table[status_idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc_table[status_idx].next = 0;

    virtio_virtq_enqueue_desc(r, q, header_idx);

    return rc;

failed:
    virtio_virtq_free_desc(q, header_idx);
    virtio_virtq_free_desc(q, body_idx);
    virtio_virtq_free_desc(q, status_idx);
    return rc;
}

void VirtioBlockDevice::cleanup_block_request(VirtioBlockRequest *req)
{
    if (req == nullptr)
        return;

    auto lock = irq_lock();
    if (req->next) {
        m_requests.remove(req);
    }
    release(lock);

    for (unsigned i = 0; i < array_size(req->descriptor_idx); i++) {
        if (req->descriptor_idx[i] == req->queue->size)
            continue;
        virtio_virtq_free_desc(m_vqueue, req->descriptor_idx[i]);
    }
}

void VirtioBlockDevice::process_used_buffer(SplitVirtQueue *q, uint32_t idx)
{
    (void) q;

    auto *req = m_requests.find_first([&](VirtioBlockRequest *req) { return req->descriptor_idx[0] == idx; });
    if (!req) {
        LOGE("Received result for descriptor %u but no request with that descriptor was found", idx);
        panic("Failed to find request with descriptor %u", idx);
    }

    LOGI("Received result for descriptor %u", idx);
    spinlock_release(req->completed);
    m_requests.remove(req);
}

void VirtioBlockDevice::handle_irq()
{
    static constexpr uint32_t VIRTIO_IRQ_USED_BUFFER = 0x00000001;

    uint32_t irq_status = ioread32(&r->InterruptStatus);
    if (irq_status & VIRTIO_IRQ_USED_BUFFER) {
        SplitVirtQueue *q = m_vqueue;
        uint16_t used = q->used->idx % q->size;
        for (uint16_t idx = q->last_seen_used_idx; idx != used; idx = (idx + 1) % q->size) {
            process_used_buffer(q, q->used->ring[idx].id);
        }
        q->last_seen_used_idx = used;
    }
    iowrite32(&r->InterruptAck, 0b11);
}

static Spinlock s_read_tempbuffer_lock = SPINLOCK_START;
static uint8_t s_read_tempbuffer[512]
__attribute__((aligned(512)));

int64_t VirtioBlockDevice::read_sector(int64_t sector_idx, uint8_t *buffer)
{
    int rc = 0;
    VirtioBlockRequest req;

    spinlock_take(s_read_tempbuffer_lock);

    LOGI("Reading sector %" PRId64, sector_idx);
    rc = enqueue_block_request(VIRTIO_BLK_T_IN, sector_idx, s_read_tempbuffer, &req);
    if (rc != 0) {
        LOGE("Failed to enqueue block request: %d", rc);
        goto cleanup;
    }
    rc = spinlock_take_with_timeout(req.completed, 100);
    if (rc != 0) {
        LOGE("Timed out waiting for request to complete");
        rc = -ETIMEDOUT;
    } else if (req.req.footer.status != 0) {
        rc = -EIO;
        LOGE("Failed to read sector %" PRId64 ": virtio returned status %d", sector_idx, req.req.footer.status);
    } else {
        /**
         * Unfortunately we cannot pass the user's buffer to virtio directly because
         * if the buffer crosses between 2 pages then it's not guaranteed that the
         * buffer was virtually-mapped to those 2 consecutive pages.
         * 
         * Ideally we would split virtio's body descriptor into 2 parts for the 
         * 2 different pages if we detect that the buffer crosses between 2 pages.
         * This is not yet implemented though, so the slow CPU copy will do.
         */
        memcpy(buffer, s_read_tempbuffer, 512);
    }

cleanup:
    cleanup_block_request(&req);
    spinlock_release(s_read_tempbuffer_lock);
    return rc;
}

int64_t VirtioBlockDevice::write_sector(int64_t, uint8_t const*)
{
    return -ENOTSUP;
}

int32_t VirtioBlockDevice::ioctl(uint32_t, void*)
{
    return -ENOTSUP;
}
