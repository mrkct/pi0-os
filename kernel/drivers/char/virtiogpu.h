#pragma once

#include <kernel/drivers/device.h>
#include <kernel/drivers/bus/virtio/virtio.h>


struct VirtioGPURequest {
    INTRUSIVE_LINKED_LIST_HEADER(VirtioGPURequest);
    uint32_t head_descriptor_idx;
    Mutex completed;
};

class VirtioGPU: public FramebufferDevice
{
public:
    struct Config {
        uintptr_t address;
        uint32_t irq;
    };

    VirtioGPU(Config const *config)
        : FramebufferDevice(), m_config(*config)
    {}

    virtual int32_t init() override;
    virtual int32_t shutdown() override;

    virtual FramebufferDevice::DisplayInfo display_info() const override { return m_displayinfo; }
    virtual int32_t refresh() override; 

private:
    int32_t device_specific_init();
    void handle_irq();

    int32_t setup_framebuffer();

    int32_t cmd_read_display_info(virtio_gpu_rect*);
    int32_t cmd_resource_create_2d(uint32_t id, uint32_t pixelformat, uint32_t width, uint32_t height);
    int32_t cmd_resource_attach_backing(uint32_t id, uintptr_t paddr, uint32_t length);
    int32_t cmd_set_scanout(virtio_gpu_rect rect, uint32_t resource_id, uint32_t scanout_id);
    int32_t cmd_transfer_to_host_2d(uint32_t resource_id, virtio_gpu_rect rect);
    int32_t cmd_resource_flush(uint32_t resource_id, virtio_gpu_rect rect);

    int32_t cmd_send_receive(
        void *cmd, size_t cmd_size,
        void *resp, size_t resp_size
    );

    Config m_config;
    VirtioRegisterMap volatile *r;
    SplitVirtQueue *m_controlq = nullptr;
    SplitVirtQueue *m_cursorq = nullptr;

    FramebufferDevice::DisplayInfo m_displayinfo = {};
    IntrusiveLinkedList<VirtioGPURequest> m_pending_requests;
};
