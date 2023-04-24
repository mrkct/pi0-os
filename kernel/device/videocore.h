#pragma once

#include <kernel/device/bus/mailbox.h>

namespace kernel {

Error get_board_revision(uint32_t& revision);
char const* get_display_name_from_board_revision_id(uint32_t revision);
Error get_firmware_revision(uint32_t& revision);

struct Framebuffer {
    size_t width, height;
    size_t pitch, depth;
    uint32_t* address;
    size_t size;
};
Error allocate_framebuffer(struct Framebuffer&);

enum class ClockId : uint32_t {
    EMMC = 1,
    UART,
    ARM,
    CORE,
    V3D,
    H264,
    ISP,
    SDRAM,
    PIXEL,
    PWM,
    HEVC,
    EMMC2,
    M2MC,
    PIXEL_BVB
};
Error get_clock_rate(ClockId, uint32_t& rate);

}
