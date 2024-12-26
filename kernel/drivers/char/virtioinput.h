#pragma once

#include <kernel/drivers/device.h>
#include <kernel/drivers/bus/virtio/virtio.h>


class VirtioInputDevice: public InputDevice
{
public:
    struct Config {
        uintptr_t address;
        uint32_t irq;
    };

    VirtioInputDevice(Config const *config)
        : InputDevice(), m_config(*config)
    {}

    virtual int32_t init() override;
    virtual int32_t shutdown() override;

private:
    int32_t device_specific_init();
    void handle_irq();
    void process_event(SplitVirtQueue *q, int desc_idx);

    void mark_descriptor_as_available(int desc_idx);

    Config m_config;
    char m_name[129];
    VirtioRegisterMap volatile *r;
    SplitVirtQueue *m_eventq = nullptr;
    SplitVirtQueue *m_statusq = nullptr;

    PhysicalPage *m_eventsbuf_page = nullptr;
    virtio_input_event *m_eventsbuf = nullptr;
};
