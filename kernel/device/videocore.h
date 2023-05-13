#pragma once

#include <kernel/device/bus/mailbox.h>
#include <kernel/device/framebuffer.h>

namespace kernel {

Error get_board_revision(uint32_t& revision);
char const* get_display_name_from_board_revision_id(uint32_t revision);
Error get_firmware_revision(uint32_t& revision);

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
