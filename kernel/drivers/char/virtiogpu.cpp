#include "virtiogpu.h"

#include <kernel/timer.h>

#define LOG_ENABLED
#define LOG_TAG "VIRT-GPU"
#include <kernel/log.h>


static constexpr size_t CONTROLQ_SIZE = 4;
static constexpr size_t CURSORQ_SIZE = 4;

static constexpr uint32_t VIRTIO_RESOURCE_ID_FB = 1;
static constexpr uint32_t VIRTIO_FB_PADDING = 4;
static constexpr uint32_t PIXEL_FORMAT = VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM;
static constexpr uint32_t BYTES_PER_PIXEL = 4;


/**
 * This is a hack because it's not easy to allocate a very large contiguous
 * area in our physical memory allocator right now.
 */
static __attribute__((aligned(4096))) uint8_t g_framebuffer_storage[1920 * 1080 * BYTES_PER_PIXEL];

static int32_t alloc_framebuffer_storage(size_t size, uintptr_t *out_paddr)
{
    kassert(size <= array_size(g_framebuffer_storage));
    *out_paddr = virt2phys(reinterpret_cast<uintptr_t>(g_framebuffer_storage));
    return 0;
}

int32_t VirtioGPU::init()
{
    int32_t rc;

    r = static_cast<VirtioRegisterMap volatile*>(ioremap(m_config.address, sizeof(VirtioRegisterMap)));
    if (!r) {
        rc = -ERR_NOMEM;
        goto failed;
    }
    LOGD("Registers are mapped at 0x%p", r);

    rc = virtio_util_do_generic_init_step(r, VirtioDeviceID::GPU, 0);
    if (rc != 0) {
        LOGE("Failed generic virtio init starting step: %" PRId32, rc);
        goto failed;
    }

    rc = device_specific_init();
    if (rc != 0) {
        LOGE("Failed device specific init step: %" PRId32, rc);
        goto failed;
    }

    rc = virtio_util_complete_init_step(r);
    if (rc != 0) {
        LOGE("Failed generic virtio init completion step: %" PRId32, rc);
        goto failed;
    }

    irq_install(m_config.irq, [](InterruptFrame*, void *arg) {
        static_cast<VirtioGPU*>(arg)->handle_irq();
    }, this);
    irq_mask(m_config.irq, false);

    rc = setup_framebuffer();
    if (rc != 0) {
        LOGE("Failed to setup the framebuffer: %" PRId32, rc);
        goto failed;
    }

    rc = refresh();
    if (rc != 0) {
        LOGE("Failed to initially refresh the framebuffer: %" PRId32, rc);
        goto failed;
    }

    return 0;

failed:
    LOGE("Failed to initialize virtiogpu: %" PRId32, rc);
    virtio_util_init_failure(r);
    iounmap((void*) r, sizeof(VirtioRegisterMap));
    irq_mask(m_config.irq, true);
    r = nullptr;
    return rc;
}

int32_t VirtioGPU::refresh()
{
    int32_t rc;

    // 5.7.6.2 Device Operation: Update a framebuffer and scanout
    // Render to your framebuffer memory
    // Use VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D to update the host resource from guest memory.
    rc = cmd_transfer_to_host_2d(VIRTIO_RESOURCE_ID_FB, {
        .x = 0,
        .y = 0,
        .width = m_displayinfo.width,
        .height = m_displayinfo.height
    });
    if (rc != 0) {
        LOGE("Failed to transfer framebuffer: %" PRId32, rc);
        goto failed;
    }
    
    // Use VIRTIO_GPU_CMD_RESOURCE_FLUSH to flush the updated resource to the display.
    rc = cmd_resource_flush(VIRTIO_RESOURCE_ID_FB, {
        .x = 0,
        .y = 0,
        .width = m_displayinfo.width,
        .height = m_displayinfo.height
    });
    if (rc != 0) {
        LOGE("Failed to flush framebuffer: %" PRId32, rc);
        goto failed;
    }

    return 0;

failed:
    return rc;
}

int32_t VirtioGPU::cmd_read_display_info(virtio_gpu_rect *out_info)
{
    int32_t rc = 0;
    virtio_gpu_ctrl_hdr cmd = {
        .type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO,
        .flags = 0,
        .fence_id = 0,
        .ctx_id = 0,
        .ring_idx = 0,
        .padding = {0, 0, 0}
    };
    virtio_gpu_resp_display_info resp = {};

    rc = cmd_send_receive(&cmd, sizeof(cmd), &resp, sizeof(resp));
    if (rc != 0) {
        LOGE("Failed to send GET_DISPLAY_INFO: %" PRId32, rc);
        goto failed;
    }

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        LOGE("Response from virtiogpu to GET_DISPLAY_INFO was not okay: %" PRIu32, resp.hdr.type);
        rc = -ERR_IO;
        goto failed;
    }

    *out_info = resp.pmodes[0].r;

    return 0;
failed:
    return rc;
}

int32_t VirtioGPU::cmd_resource_create_2d(uint32_t id, uint32_t pixelformat, uint32_t width, uint32_t height)
{
    int32_t rc = 0;
    virtio_gpu_resource_create_2d cmd = {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
            .ring_idx = 0,
            .padding = {0, 0, 0}
        },
        .resource_id = id,
        .format = pixelformat,
        .width = width,
        .height = height
    };
    virtio_gpu_ctrl_hdr resp = {};
    
    rc = cmd_send_receive(&cmd, sizeof(cmd), &resp, sizeof(resp));
    if (rc != 0) {
        LOGE("Failed to send RESOURCE_CREATE_2D: %" PRId32, rc);
        goto failed;
    }

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        LOGE("Response from virtiogpu to RESOURCE_CREATE_2D was not okay: %" PRIu32, resp.type);
        rc = -ERR_IO;
        goto failed;
    }

    LOGI("Resource created: %" PRIu32, cmd.resource_id);
    return 0;
failed:
    return rc;
}

int32_t VirtioGPU::cmd_resource_attach_backing(uint32_t id, uintptr_t paddr, uint32_t length)
{
    int32_t rc = 0;
    struct {
        virtio_gpu_resource_attach_backing cmd;
        virtio_gpu_mem_entry entries[1];
    } cmd = {
        .cmd = {
            .hdr = {
                .type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
                .flags = 0,
                .fence_id = 0,
                .ctx_id = 0,
                .ring_idx = 0,
                .padding = {0, 0, 0}
            },
            .resource_id = id,
            .nr_entries = 1,
        },
        .entries = {
            [0] = {
                .addr = paddr,
                .length = length,
                .padding = VIRTIO_FB_PADDING
            }
        }
    };

    virtio_gpu_ctrl_hdr resp = {};
    rc = cmd_send_receive(&cmd, sizeof(cmd), &resp, sizeof(resp));
    if (rc != 0) {
        LOGE("Failed to send RESOURCE_ATTACH_BACKING: %" PRId32, rc);
        goto failed;
    }

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        LOGE("Response from virtiogpu to RESOURCE_ATTACH_BACKING was not okay: %" PRIu32, resp.type);
        rc = -ERR_IO;
        goto failed;
    }

    LOGI("Resource %" PRIu32 " attached", id);
    return 0;

failed:
    return rc;
}

int32_t VirtioGPU::cmd_set_scanout(virtio_gpu_rect rect, uint32_t resource_id, uint32_t scanout_id)
{
    int32_t rc = 0;
    virtio_gpu_set_scanout cmd = {
        .hdr = {
            .type = VIRTIO_GPU_CMD_SET_SCANOUT,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
            .ring_idx = 0,
            .padding = {0, 0, 0}
        },
        .r = rect,
        .scanout_id = scanout_id,
        .resource_id = resource_id,
    };
    virtio_gpu_ctrl_hdr resp = {};
    
    rc = cmd_send_receive(&cmd, sizeof(cmd), &resp, sizeof(resp));
    if (rc != 0) {
        LOGE("Failed to send SET_SCANOUT: %" PRId32, rc);
        goto failed;
    }

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        LOGE("Response from virtiogpu to SET_SCANOUT was not okay: %" PRIu32, resp.type);
        rc = -ERR_IO;
        goto failed;
    }
    
    LOGI("Scanout set");
    return 0;

failed:
    return rc;
}

int32_t VirtioGPU::cmd_transfer_to_host_2d(uint32_t resource_id, virtio_gpu_rect rect)
{
    int32_t rc = 0;
    struct virtio_gpu_transfer_to_host_2d cmd {
        .hdr = {
            .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
            .ring_idx = 0,
            .padding = {0, 0, 0}
        },
        .r = rect,
        .offset = 0,
        .resource_id = resource_id,
        .padding = VIRTIO_FB_PADDING
    };
    virtio_gpu_ctrl_hdr resp = {};
    LOGI("Transfer to host 2D for resource %" PRIu32, resource_id);
    rc = cmd_send_receive(&cmd, sizeof(cmd), &resp, sizeof(resp));
    if (rc != 0) {
        LOGE("Failed to send TRANSFER_TO_HOST_2D: %" PRId32, rc);
        goto failed;
    }

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        LOGE("Response from virtiogpu to TRANSFER_TO_HOST_2D was not okay: %" PRIu32, resp.type);
        rc = -ERR_IO;
        goto failed;
    }

    return 0;
failed:
    return rc;
}

int32_t VirtioGPU::cmd_resource_flush(uint32_t resource_id, virtio_gpu_rect rect)
{
    int32_t rc = 0;
    struct virtio_gpu_resource_flush cmd {
        .hdr = {
            .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
            .flags = 0,
            .fence_id = 0,
            .ctx_id = 0,
            .ring_idx = 0,
            .padding = {0, 0, 0}
        },
        .r = rect,
        .resource_id = resource_id,
        .padding = VIRTIO_FB_PADDING
    };
    virtio_gpu_ctrl_hdr resp = {};
    
    rc = cmd_send_receive(&cmd, sizeof(cmd), &resp, sizeof(resp));
    if (rc != 0) {
        LOGE("Failed to send RESOURCE_FLUSH: %" PRId32, rc);
        goto failed;
    }

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        LOGE("Response from virtiogpu to RESOURCE_FLUSH was not okay: %" PRIu32, resp.type);
        rc = -ERR_IO;
        goto failed;
    }

    return 0;
failed:
    return rc;
}

int32_t VirtioGPU::setup_framebuffer()
{
    int32_t rc = 0;
    uintptr_t fb_paddr;
    struct virtio_gpu_rect display0;

    // 5.7.5 Device Requirements: Device Initialization
    // The driver SHOULD query the display information from the device using
    // the VIRTIO_GPU_CMD_GET_DISPLAY_INFO command and use that information
    // for the initial scanout setup.
    // In case EDID support is negotiated (NOTE: we didn't) ...

    rc = cmd_read_display_info(&display0);
    if (rc != 0) {
        LOGE("Failed to read display info: %" PRId32, rc);
        goto failed;
    }

    LOGI("Display info: (%" PRIu32 ", %" PRIu32 ") - (%" PRIu32 ", %" PRIu32 ")",
        display0.x, display0.y, display0.width, display0.height
    );

    rc = alloc_framebuffer_storage(BYTES_PER_PIXEL * display0.width * display0.height, &fb_paddr);
    if (rc != 0) {
        LOGE("Failed to allocate framebuffer storage: %" PRId32, rc);
        goto failed;
    }

    m_displayinfo = FramebufferDevice::DisplayInfo {
        .width = display0.width,
        .height = display0.height,
        .bytes_per_pixel = BYTES_PER_PIXEL,
        .pitch = BYTES_PER_PIXEL * display0.width,
        .fb_phys_addr = fb_paddr,
    };
    
    // 5.7.6.1 Device Operation: Create a framebuffer and configure scanout
    
    // Create a host resource using VIRTIO_GPU_CMD_RESOURCE_CREATE_2D.
    rc = cmd_resource_create_2d(VIRTIO_RESOURCE_ID_FB, PIXEL_FORMAT, m_displayinfo.width, m_displayinfo.height);
    if (rc != 0) {
        LOGE("Failed to create resource: %" PRId32, rc);
        goto failed;
    }

    // Allocate a framebuffer from guest ram, and attach it as backing storage
    // to the resource just created, using VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING.
    // Scatter lists are supported, so the framebuffer doesn’t need to be contignous
    // in guest physical memory.
    rc = cmd_resource_attach_backing(VIRTIO_RESOURCE_ID_FB, m_displayinfo.fb_phys_addr, m_displayinfo.fb_length());
    if (rc != 0) {
        LOGE("Failed to attach backing: %" PRId32, rc);
        goto failed;
    }

    // Use VIRTIO_GPU_CMD_SET_SCANOUT to link the framebuffer to a display scanout.
    rc = cmd_set_scanout(display0, VIRTIO_RESOURCE_ID_FB, 0);
    if (rc != 0) {
        LOGE("Failed to set scanout: %" PRId32, rc);
        goto failed;
    }

    LOGI("Framebuffer set up");

    return 0;

failed:
    if (fb_paddr != 0) {
        LOGW("Freeing framebuffer storage is not implemented yet");
    }
    return rc;
}

int32_t VirtioGPU::device_specific_init()
{
    int32_t rc = 0;

    rc = virtio_util_setup_virtq(r, 0, CONTROLQ_SIZE, &m_controlq);
    if (rc) {
        LOGE("Failed to setup controlq virtq: %" PRId32, rc);
        goto failed;
    }

    rc = virtio_util_setup_virtq(r, 1, CURSORQ_SIZE, &m_cursorq);
    if (rc) {
        LOGE("Failed to setup cursorq virtq: %" PRId32, rc);
        goto failed;
    }

    return 0;

failed:
    panic("freeing the virtq is not implemented");
    return rc;
}

int32_t VirtioGPU::shutdown()
{
    todo();
    return 0;
}

void VirtioGPU::handle_irq()
{
    static constexpr uint32_t VIRTIO_IRQ_USED_BUFFER = 0x00000001;
    
    uint32_t irq_status = ioread32(&r->InterruptStatus);
    if (irq_status & VIRTIO_IRQ_USED_BUFFER) {
        m_controlq->foreach_used_descriptor([&](auto*, uint32_t desc_idx) {
            VirtioGPURequest *req = m_pending_requests.find_first([&](VirtioGPURequest *req) {
                return req->head_descriptor_idx == desc_idx;
            });
            if (req == nullptr) {
                LOGW("Failed to find request for descriptor idx %" PRIu32 "(time out?)", desc_idx);
                return;
            }

            m_pending_requests.remove(req);
            mutex_release(req->completed);
        });
    }
    iowrite32(&r->InterruptAck, 0b11);
}

int32_t VirtioGPU::cmd_send_receive(
    void *cmd, size_t cmd_size,
    void *resp, size_t resp_size
)
{
    int32_t rc = 0;
    int cmd_desc_idx = -1, resp_desc_idx = -1;
    uintptr_t cmd_pa_addr, resp_pa_addr;
    SplitVirtQueue *q = m_controlq;
    PhysicalPage *storage = nullptr;
    VirtioGPURequest request = {};

    kassert(round_up(cmd_size, 8) + resp_size < 4096);

    cmd_desc_idx = virtio_virtq_alloc_desc(m_controlq);
    resp_desc_idx = virtio_virtq_alloc_desc(m_controlq);
    if (cmd_desc_idx < 0 || resp_desc_idx < 0) {
        LOGE("Failed to allocate descriptors for the command");
        rc = -ERR_NOMEM;
        goto cleanup;
    }

    if (!physical_page_alloc(PageOrder::_4KB, storage).is_success()) {
        LOGE("Failed to allocate storage page for the command/response");
        rc = -ERR_NOMEM;
        goto cleanup;
    }

    cmd_pa_addr = page2addr(storage);
    resp_pa_addr = page2addr(storage) + round_up(cmd_size, 8);

    memcpy((void*) phys2virt(cmd_pa_addr), cmd, cmd_size);

    q->desc_table[cmd_desc_idx].addr = cmd_pa_addr;
    q->desc_table[cmd_desc_idx].len = cmd_size;
    q->desc_table[cmd_desc_idx].flags = VIRTQ_DESC_F_NEXT;
    q->desc_table[cmd_desc_idx].next = (le16) resp_desc_idx;

    q->desc_table[resp_desc_idx].addr = resp_pa_addr;
    q->desc_table[resp_desc_idx].len = resp_size;
    q->desc_table[resp_desc_idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc_table[resp_desc_idx].next = 0;

    request.head_descriptor_idx = cmd_desc_idx;
    mutex_init(request.completed, MutexInitialState::Locked);
    {
        auto lock = irq_lock();
        m_pending_requests.add(&request);
        release(lock);
    }

    virtio_virtq_enqueue_desc(r, q, cmd_desc_idx);
    mutex_take(request.completed);
    memcpy(resp, (void*) phys2virt(resp_pa_addr), resp_size);

cleanup:
    if (cmd_desc_idx >= 0)
        virtio_virtq_free_desc(m_controlq, cmd_desc_idx);
    if (resp_desc_idx >= 0)
        virtio_virtq_free_desc(m_controlq, resp_desc_idx);
    if (storage != nullptr)
        physical_page_free(storage, PageOrder::_4KB);

    return rc;
}
