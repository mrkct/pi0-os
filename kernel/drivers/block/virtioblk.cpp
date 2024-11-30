#include "virtioblk.h"
#include <kernel/lib/arrayutils.h>
#include <kernel/locking/irqlock.h>

#define LOG_ENABLED
#define LOG_TAG "VBLK"
#include <kernel/log.h>


static constexpr uint32_t VIRTIO_MAGIC = 0x74726976;
static constexpr uint32_t VIRTIO_VERSION = 2;

static constexpr uint32_t VIRTQ_SIZE = 64;

enum class VirtioDeviceID: uint32_t {
    Invalid,
    BlockDevice = 2,
};

enum VirtioDeviceStatus: uint32_t {
    Acknowledge = 1 << 0,
    Driver = 1 << 1,
    DriverOk = 1 << 2,
    FeaturesOk = 1 << 3,
    DeviceNeedsReset = 1 << 6,
    Failed = 1 << 7,
};

enum VirtioBlockDeviceFeatures: uint32_t {
    ReadOnly = 1 << 5,
};

static constexpr uint32_t VIRTIO_SUPPORTED_FEATURES = 0;
static constexpr uint32_t BLOCK_DEVICE_SUPPORTED_FEATURES = VirtioBlockDeviceFeatures::ReadOnly;

static SplitVirtQueue *virtq_alloc(size_t idx, size_t size);
static void virtq_free(SplitVirtQueue *q);
static int virtq_alloc_desc(SplitVirtQueue *q);
static void virtq_free_desc(SplitVirtQueue *q, int desc_idx);


static SplitVirtQueue *virtq_alloc(size_t idx, size_t size)
{
    // 2.6 Split Virtqueues
    // The memory alignment and size requirements, in bytes,
    // of each part of the virtqueue are summarized in the
    // following table:
    //
    // | Virtqueue Part   | Alignment | Size                 |
    // +------------------+----------------------------------+
    // | Descriptor Table | 16        | 16*(Queue Size)      |
    // | Available Ring   |  2        | 6 + 2*(Queue Size)   |
    // | Used Ring        |  4        | 6 + 8 * (Queue Size) |

    PhysicalPage *page;
    if (!physical_page_alloc(PageOrder::_4KB, page).is_success())
        return nullptr;
    
    uint8_t *mem = (uint8_t*) phys2virt(page2addr(page));
    LOGD("Virtqueue of size %u is allocated at 0x%p", size, mem);
    uint8_t *end = mem + _4KB;

    uint8_t *desc_table = mem;
    mem += 16 * size;
    uint8_t *avail = mem;
    mem += round_up<size_t>(6 + 2 * size, 8);
    uint8_t *used = mem;
    mem += round_up<size_t>(6 + 8 * size, 8);

    SplitVirtQueue *q = (SplitVirtQueue*) mem;
    mem += sizeof(SplitVirtQueue);

    if (mem >= end) {
        panic("too big");
    }

    *q = (SplitVirtQueue){
        .idx = idx,
        .size = size,
        .alloc_page = page,
        .first_free_desc_idx = 0,
        .last_seen_used_idx = 0,
        .desc_table = (struct virtq_desc*) desc_table,
        .avail = (struct virtq_avail*) avail,
        .used = (struct virtq_used*) used,
    };
    for (size_t i = 0; i < size; i++) {
        q->desc_table[i].addr = 0;
        q->desc_table[i].len = 0;
        q->desc_table[i].flags = 0;
        q->desc_table[i].next = i + 1;
    }

    return q;
}

static void virtq_free(SplitVirtQueue *q)
{
    if (q == nullptr)
        return;
    physical_page_free(q->alloc_page, PageOrder::_4KB);
}

static int virtq_alloc_desc(SplitVirtQueue *q)
{
    if (q->first_free_desc_idx >= q->size)
        return -ENOMEM;
    
    int desc_idx = q->first_free_desc_idx;
    q->first_free_desc_idx = q->desc_table[desc_idx].next;
    memory_barrier();
    return desc_idx;
}

static void virtq_free_desc(SplitVirtQueue *q, int desc_idx)
{
    if (desc_idx < 0 || desc_idx >= (int) q->size)
        return;

    q->desc_table[desc_idx].addr = 0;
    q->desc_table[desc_idx].len = 0;
    q->desc_table[desc_idx].flags = 0;
    q->desc_table[desc_idx].next = (le16) q->first_free_desc_idx;
    q->first_free_desc_idx = desc_idx;
    memory_barrier();
}

bool VirtioBlockDevice::probe(uintptr_t address)
{
    VirtioRegisterMap volatile *r = reinterpret_cast<VirtioRegisterMap volatile*>(ioremap(address, sizeof(VirtioRegisterMap)));
    if (!r)
        return false;
    
    bool result = false;
    if (ioread32(&r->MagicValue) == VIRTIO_MAGIC && ioread32(&r->DeviceID) != 0) {
        result = true;
    }
    iounmap((void*) r, sizeof(VirtioRegisterMap));
    return result;
}

int32_t VirtioBlockDevice::init()
{
    int32_t rc = 0;
    uint32_t features = 0;

    r = static_cast<VirtioRegisterMap volatile*>(ioremap(m_config.address, sizeof(VirtioRegisterMap)));
    if (!r) {
        rc = -ENOMEM;
        goto failed;
    }
    LOGD("Registers are mapped at 0x%p", r);

    // 4.2.3 MMIO-specific Initialization And Device Operation
    // 4.2.3.1 Device Initialization
    // 4.2.3.1.1 Driver Requirements: Device Initialization

    // "The driver MUST start the device initialization by reading and
    //  checking values from MagicValue and Version ..."
    if (ioread32(&r->MagicValue) != VIRTIO_MAGIC) {
        LOGE("Invalid Magic Value");
        rc = -ENODEV;
        goto failed;
    } else if (ioread32(&r->Version) != VIRTIO_VERSION) {
        LOGE("Invalid VirtIO version (expected %d, read %d)",
             VIRTIO_VERSION, ioread32(&r->Version));
        rc = -ENOTSUP;
        goto failed;
    }

    // "... If both values are valid, it MUST read DeviceID and if its
    //  value is zero (0x0) MUST abort initialization and MUST NOT access
    //  any other register..."
    if (ioread32(&r->DeviceID) == 0) {
        LOGE("No VirtIO device found at address %p", m_config.address);
        rc = -ENODEV;
        goto failed;
    }

    // "... Further initialization MUST follow the procedure described in
    // 3.1 Device Initialization"
    // 3.1.1 Driver Requirements: Device Initialization

    // 1. Reset the device.
    iowrite32(&r->Status, 0);

    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Acknowledge);
    
    // 3. Set the DRIVER status bit: the guest OS knows how to drive the device.
    if (ioread32(&r->DeviceID) != static_cast<uint32_t>(VirtioDeviceID::BlockDevice)) {
        LOGE("Not a block device");
        rc = -ENOTSUP;
        goto failed;
    }
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Driver);
    
    // 4. Read device feature bits, and write the subset of feature bits understood
    //    by the OS and driver to the device.
    //    During this step the driver MAY read (but MUST NOT write) the device-specific
    //    configuration fields to check that it can support the device before accepting it.
    features = 0;
    iowrite32(&r->DeviceFeaturesSel, 0);
    features = ioread32(&r->DeviceFeatures);
    features &= BLOCK_DEVICE_SUPPORTED_FEATURES;

    iowrite32(&r->DriverFeaturesSel, 0);
    iowrite32(&r->DriverFeatures, features);

    // 5. Set the FEATURES_OK status bit. The driver MUST NOT accept new feature bits after
    //    this step.
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::FeaturesOk);

    // 6. Re-read device status to ensure the FEATURES_OK bit is still set: otherwise, the
    //    device does not support our subset of features and the device is unusable.
    if (!(ioread32(&r->Status) & VirtioDeviceStatus::FeaturesOk)) {
        LOGE("Device does not support our feature set");
        rc = -ENOTSUP;
        goto failed;
    }

    // 7. Perform device-specific setup, including discovery of virtqueues for the device,
    //    optional per-bus setup, reading and possibly writing the device’s virtio
    //    configuration space, and population of virtqueues.
    rc = block_device_init();
    if (rc)
        goto failed;

    // 8. Set the DRIVER_OK status bit. At this point the device is “live”.
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::DriverOk);
    m_ready = true;
    
    irq_install(m_config.irq, [](InterruptFrame*, void *arg) {
        static_cast<VirtioBlockDevice*>(arg)->handle_irq();
    }, this);
    irq_mask(m_config.irq, false);

    return 0;

failed:
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Failed);
    iounmap((void*) r, sizeof(VirtioRegisterMap));
    r = nullptr;
    return rc;
}

int32_t VirtioBlockDevice::init_virtqueue(uint32_t index, uint32_t size)
{
    int32_t rc = 0;
    size_t queue_size = 0;
    SplitVirtQueue *q = nullptr;
    uint64_t desc_table, avail, used;

    // 4.2.3.2 Virtqueue Configuration
    // A block device has a single virtqueue

    // 1. Select the queue writing its index (first queue is 0) to QueueSel.
    iowrite32(&r->QueueSel, index);

    // 2. Check if the queue is not already in use: read QueueReady,
    //    and expect a returned value of zero (0x0).
    if (ioread32(&r->QueueReady) != 0) {
        LOGE("Queue already in use");
        rc = -EBUSY;
        goto failed;
    }

    // 3. Read maximum queue size (number of elements) from QueueNumMax.
    //    If the returned value is zero (0x0) the queue is not available.
    queue_size = min(ioread32(&r->QueueNumMax), size);
    if (queue_size == 0) {
        LOGE("Queue not available");
        rc = -ENOMEM;
        goto failed;
    }

    // 4. Allocate and zero the queue memory, making sure the memory is
    //    physically contiguous
    q = virtq_alloc(index, queue_size);
    if (q == nullptr) {
        LOGE("Failed to allocate virtqueue");
        rc = -ENOMEM;
        goto failed;
    }
    m_vqueue = q;

    // 5. Notify the device about the queue size by writing the size to QueueNum
    iowrite32(&r->QueueNum, queue_size);

    // 6. Write physical addresses of the queue’s Descriptor Area, Driver Area
    //    and Device Area to (respectively) the QueueDescLow/QueueDescHigh,
    //    QueueDriverLow/QueueDriverHigh and QueueDeviceLow/QueueDeviceHigh
    //    register pairs.
    desc_table = (uint64_t) virt2phys((uintptr_t) q->desc_table);
    avail = (uint64_t) virt2phys((uintptr_t) q->avail);
    used = (uint64_t) virt2phys((uintptr_t) q->used);
    iowrite32(&r->QueueDescLow,    (desc_table >> 0) & 0xffffffff);
    iowrite32(&r->QueueDescHigh,   (desc_table >> 32) & 0xffffffff);
    iowrite32(&r->QueueDriverLow,  (avail >> 0) & 0xffffffff);
    iowrite32(&r->QueueDriverHigh, (avail >> 32) & 0xffffffff);
    iowrite32(&r->QueueDeviceLow,  (used >> 0) & 0xffffffff);
    iowrite32(&r->QueueDeviceHigh, (used >> 32) & 0xffffffff);

    // 7. Write 0x1 to QueueReady
    iowrite32(&r->QueueReady, 1);

    return rc;

failed:
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Failed);
    virtq_free(q);
    return rc;
}

int32_t VirtioBlockDevice::block_device_init()
{
    int32_t rc;
    
    rc = init_virtqueue(0, VIRTQ_SIZE);
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

    header_idx = virtq_alloc_desc(q);
    body_idx = virtq_alloc_desc(q);
    status_idx = virtq_alloc_desc(q);
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

    LOGD("q->avail->ring[%d] = %d", (int) q->avail->idx, header_idx);
    q->avail->ring[q->avail->idx % q->size] = (le16) header_idx;
    memory_barrier();
    q->avail->idx = q->avail->idx + 1;
    memory_barrier();
    iowrite32(&r->QueueNotify, m_vqueue->idx);

    return rc;

failed:
    virtq_free_desc(q, header_idx);
    virtq_free_desc(q, body_idx);
    virtq_free_desc(q, status_idx);
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
        virtq_free_desc(m_vqueue, req->descriptor_idx[i]);
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
