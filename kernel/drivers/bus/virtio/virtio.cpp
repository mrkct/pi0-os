#include <kernel/memory/vm.h>
#include "virtio.h"

#define LOG_ENABLED
#define LOG_TAG "VIRTIO"
#include <kernel/log.h>


static constexpr uint32_t VIRTIO_MAGIC = 0x74726976;
static constexpr uint32_t VIRTIO_VERSION = 2;

enum VirtioDeviceStatus: uint32_t {
    Acknowledge = 1 << 0,
    Driver = 1 << 1,
    DriverOk = 1 << 2,
    FeaturesOk = 1 << 3,
    DeviceNeedsReset = 1 << 6,
    Failed = 1 << 7,
};

static SplitVirtQueue *virtq_alloc(size_t idx, size_t size);
static void virtq_free(SplitVirtQueue *q);


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

VirtioDeviceID virtio_util_probe(uintptr_t address)
{
    VirtioRegisterMap volatile *r = reinterpret_cast<VirtioRegisterMap volatile*>(ioremap(address, sizeof(VirtioRegisterMap)));
    if (!r) {
        LOGE("Failed to iomap for probing");
        return VirtioDeviceID::Invalid;
    }

#define SUPPORTED(e) case static_cast<uint32_t>(e): { devid = e; break; }

    VirtioDeviceID devid = VirtioDeviceID::Invalid;
    if (ioread32(&r->MagicValue) == VIRTIO_MAGIC) {
        switch (ioread32(&r->DeviceID)) {
            SUPPORTED(VirtioDeviceID::BlockDevice)
            SUPPORTED(VirtioDeviceID::Invalid)
            default: {
                devid = VirtioDeviceID::Unsupported;
                break;
            }
        }
    }
    iounmap((void*) r, sizeof(VirtioRegisterMap));
    return devid;
}

int32_t virtio_util_do_generic_init_step(
    VirtioRegisterMap volatile *r,
    VirtioDeviceID expected_device_class,
    uint32_t supported_features
)
{
    int32_t rc = 0;
    uint32_t features = 0;

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
        LOGE("No VirtIO device found in register map");
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
    if (ioread32(&r->DeviceID) != static_cast<uint32_t>(expected_device_class)) {
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
    features &= supported_features;

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

    return 0;
failed:
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Failed);
    return rc;
}

int32_t virtio_util_complete_init_step(VirtioRegisterMap volatile *r)
{
    // ... continues from the init in virtio_util_do_generic_unit_step

    // 8. Set the DRIVER_OK status bit. At this point the device is “live”.
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::DriverOk);
    return 0;
}

int32_t virtio_util_init_failure(VirtioRegisterMap volatile *r)
{
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Failed);
    return 0;
}

int32_t virtio_util_setup_virtq(
    VirtioRegisterMap volatile *r,
    uint32_t index, uint32_t size,
    SplitVirtQueue **out_q
)
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
    *out_q = q;

    return rc;

failed:
    virtq_free(q);
    return rc;
}

int virtio_virtq_alloc_desc(SplitVirtQueue *q)
{
    if (q->first_free_desc_idx >= q->size)
        return -ENOMEM;
    
    int desc_idx = q->first_free_desc_idx;
    q->first_free_desc_idx = q->desc_table[desc_idx].next;
    memory_barrier();
    return desc_idx;
}

void virtio_virtq_free_desc(SplitVirtQueue *q, int desc_idx)
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

void virtio_virtq_enqueue_desc(
    VirtioRegisterMap volatile *r,
    SplitVirtQueue *q, int desc_idx
)
{
    q->avail->ring[q->avail->idx % q->size] = (le16) desc_idx;
    memory_barrier();
    q->avail->idx = q->avail->idx + 1;
    memory_barrier();
    iowrite32(&r->QueueNotify, q->idx);
}
