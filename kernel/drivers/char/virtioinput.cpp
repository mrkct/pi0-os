#include "virtioinput.h"

// #define LOG_ENABLED
#define LOG_TAG "VINPUT"
#include <kernel/log.h>


static constexpr size_t EVENTQ_SIZE = 4;
static constexpr size_t STATUSQ_SIZE = 4;


int32_t VirtioInputDevice::init()
{
    int32_t rc;

    r = static_cast<VirtioRegisterMap volatile*>(ioremap(m_config.address, sizeof(VirtioRegisterMap)));
    if (!r) {
        rc = -ERR_NOMEM;
        goto failed;
    }
    LOGD("Registers are mapped at 0x%p", r);

    rc = virtio_util_do_generic_init_step(r, VirtioDeviceID::InputDevice, 0);
    if (rc != 0) {
        LOGE("Failed generic virtio init starting step: %" PRId32);
        goto failed;
    }

    rc = device_specific_init();
    if (rc != 0) {
        LOGE("Failed device specific init step: %" PRId32);
        goto failed;
    }

    rc = virtio_util_complete_init_step(r);
    if (rc != 0) {
        LOGE("Failed generic virtio init completion step: %" PRId32);
        goto failed;
    }

    // Mark all the descriptors as available for the device to write events to
    for (size_t i = 0; i < m_eventq->size - 1; i++) {
        int tempdesc = virtio_virtq_alloc_desc(m_eventq);
        kassert(tempdesc >= 0);
        mark_descriptor_as_available(tempdesc);
    }

    irq_install(m_config.irq, [](InterruptFrame*, void *arg) {
        static_cast<VirtioInputDevice*>(arg)->handle_irq();
    }, this);
    irq_mask(m_config.irq, false);

    return 0;

failed:
    virtio_util_init_failure(r);
    iounmap((void*) r, sizeof(VirtioRegisterMap));
    r = nullptr;
    return rc;
}


void VirtioInputDevice::mark_descriptor_as_available(int desc_idx)
{
    SplitVirtQueue *q = m_eventq;
    m_eventsbuf[desc_idx] = {};

    uintptr_t eventbuf = reinterpret_cast<uintptr_t>(&m_eventsbuf[desc_idx]);
    q->desc_table[desc_idx].addr = virt2phys(eventbuf);
    q->desc_table[desc_idx].len = sizeof(virtio_input_event);
    q->desc_table[desc_idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc_table[desc_idx].next = 0;

    virtio_virtq_enqueue_desc(r, q, desc_idx);
}

int32_t VirtioInputDevice::device_specific_init()
{
    int32_t rc = 0;

    iowrite8(&r->input.select, VIRTIO_INPUT_CFG_ID_NAME);
    iowrite8(&r->input.subsel, 0);

    uint8_t name_len = ioread8(&r->input.size);
    if (name_len >= sizeof(m_name)) {
        LOGW("Device name too long (%" PRIu8 "), truncating", name_len);
        name_len = sizeof(m_name) - 1;
    }
    for (uint8_t i = 0; i < name_len; ++i) {
        m_name[i] = ioread8(&r->input.u.string[i]);
    }
    m_name[name_len] = '\0';
    LOGI("Device name: %s", m_name);

    rc = virtio_util_setup_virtq(r, 0, EVENTQ_SIZE, &m_eventq);
    if (rc) {
        LOGE("Failed to setup eventq virtq: %" PRId32, rc);
        goto failed;
    }

    static_assert(4 * _1KB / sizeof(virtio_input_event) >= EVENTQ_SIZE);
    if (!physical_page_alloc(PageOrder::_4KB, m_eventsbuf_page).is_success()) {
        LOGE("Failed to alloc memory for storing the received input buffers");
        goto failed;
    }
    m_eventsbuf = reinterpret_cast<virtio_input_event*>(phys2virt(page2addr(m_eventsbuf_page)));

    rc = virtio_util_setup_virtq(r, 1, STATUSQ_SIZE, &m_statusq);
    if (rc) {
        LOGE("Failed to setup statusq virtq: %" PRId32, rc);
        goto failed;
    }

    

    return 0;

failed:
    if (m_eventsbuf)
        free(m_eventsbuf);

    panic("freeing the virtq is not implemented");
    return rc;
}

int32_t VirtioInputDevice::shutdown()
{
    todo();
    return 0;
}

void VirtioInputDevice::process_event(SplitVirtQueue*, int desc_idx)
{
    virtio_input_event event = m_eventsbuf[desc_idx];
    m_eventsbuf[desc_idx] = {};
    LOGD("Event { Type: %" PRIu16 ", Code: %" PRIu16 ", Value: %" PRIu32 "}", event.type, event.code, event.value);

    switch (event.type) {
        case EV_SYN: {
            // don't care
            break;
        }

        case EV_KEY: {
            notify_event(api::InputEvent {
                .type = EV_KEY,
                .key = {
                    .code = event.code,
                    .pressed = event.value
                }
            });
            break;
        }

        default: {
            LOGW("Unknown event type %" PRIu16, event.type);
        }
    }

    mark_descriptor_as_available(desc_idx);
}

void VirtioInputDevice::handle_irq()
{
    static constexpr uint32_t VIRTIO_IRQ_USED_BUFFER = 0x00000001;
    
    uint32_t irq_status = ioread32(&r->InterruptStatus);
    if (irq_status & VIRTIO_IRQ_USED_BUFFER) {
        m_eventq->foreach_used_descriptor([&](auto *q, uint16_t idx) { process_event(q, idx); });
        // We never send any commands, so we should never get updates in the status queue
    }
    iowrite32(&r->InterruptAck, 0b11);
}
