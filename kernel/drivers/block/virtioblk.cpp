#include "virtioblk.h"

#define LOG_ENABLED
#define LOG_TAG "[VBLK] "
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

enum VirtioDeviceFeatures: uint32_t {

};

enum VirtioBlockDeviceFeatures: uint32_t {
    ReadOnly = 1 << 5,
};

static constexpr uint32_t VIRTIO_SUPPORTED_FEATURES = 0;
static constexpr uint32_t BLOCK_DEVICE_SUPPORTED_FEATURES = VirtioBlockDeviceFeatures::ReadOnly;

static SplitVirtQueue *virtqueue_alloc(size_t size)
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
    LOGD("Virtqueue is allocated at 0x%p", mem);
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
        .size = size,
        .alloc_page = page,
        .desc_table = (struct virtq_desc*) desc_table,
        .avail = (struct virtq_avail*) avail,
        .used = (struct virtq_used*) used,
    };
    return q;
}

static void virtqueue_free(SplitVirtQueue *q)
{
    if (!q)
        return;
    physical_page_free(q->alloc_page, PageOrder::_4KB);
}

bool VirtioBlockDevice::probe(uintptr_t address)
{
    RegisterMap volatile *r = reinterpret_cast<RegisterMap volatile*>(ioremap(address, sizeof(RegisterMap)));
    if (!r)
        return false;
    
    bool result = false;
    if (ioread32(&r->MagicValue) == VIRTIO_MAGIC && ioread32(&r->DeviceID) != 0) {
        result = true;
    }
    iounmap((void*) r, sizeof(RegisterMap));
    return result;
}

int32_t VirtioBlockDevice::init()
{
    int32_t rc = 0;
    uint32_t features = 0;

    r = static_cast<RegisterMap volatile*>(ioremap(m_config.address, sizeof(RegisterMap)));
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

    return 0;

failed:
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Failed);
    iounmap((void*) r, sizeof(RegisterMap));
    r = nullptr;
    return rc;
}

int32_t VirtioBlockDevice::init_virtqueue(uint32_t index, uint32_t size)
{
    int32_t rc = 0;
    size_t queue_size = 0;
    SplitVirtQueue *q = nullptr;

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
    q = virtqueue_alloc(queue_size);
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
    iowrite32(&r->QueueDescLow,    ((uint64_t) q->desc_table >> 0) & 0xffffffff);
    iowrite32(&r->QueueDescLow,    ((uint64_t) q->desc_table >> 32) & 0xffffffff);
    iowrite32(&r->QueueDriverLow,  ((uint64_t) q->avail >> 0) & 0xffffffff);
    iowrite32(&r->QueueDriverHigh, ((uint64_t) q->avail >> 32) & 0xffffffff);
    iowrite32(&r->QueueDeviceLow,  ((uint64_t) q->used >> 0) & 0xffffffff);
    iowrite32(&r->QueueDeviceHigh, ((uint64_t) q->used >> 32) & 0xffffffff);

    // 7. Write 0x1 to QueueReady
    iowrite32(&r->QueueReady, 1);

failed:
    iowrite32(&r->Status, ioread32(&r->Status) | VirtioDeviceStatus::Failed);
    virtqueue_free(q);
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

int64_t VirtioBlockDevice::read(int64_t offset, uint8_t *buffer, size_t size)
{
    if (!m_ready)
        return -ENODEV;
    
    offset = clamp<int64_t>(0, offset, m_capacity);
    size = clamp<int64_t>(0, size, m_capacity - offset);

    (void) offset;
    (void) buffer;
    (void) size;
    return -ENOTSUP;
}

int64_t VirtioBlockDevice::write(int64_t offset, const uint8_t *buffer, size_t size)
{
    if (!m_ready)
        return -ENODEV;

    if (m_readonly)
        return -EPERM;
    
    (void) offset;
    (void) buffer;
    (void) size;
    return -ENOTSUP;
}

int32_t VirtioBlockDevice::ioctl(uint32_t request, void *argp)
{
    (void) request;
    (void) argp;
    return -ENOTSUP;
}
