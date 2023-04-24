#include <kernel/device/bus/mailbox.h>
#include <kernel/device/videocore.h>
#include <kernel/lib/memory.h>

namespace kernel {

/*
    NOTE: QEMU doesn't implement(or only has stubs) many of the mailbox tags like GetCommandLine or GetBoardModel
    See: https://github.com/qemu/qemu/blob/60ca584b8af0de525656f959991a440f8c191f12/hw/misc/bcm2835_property.c
*/

static Error four_bytes_message(Channel channel, uint32_t tag_id, uint32_t& data)
{
    struct __attribute__((aligned(16))) {
        MailboxMessageHeader header;
        struct {
            uint32_t tag_id;
            uint32_t buffer_size;
            uint32_t request_response_codes_and_data_size;
            uint32_t data;
        } tags;
        MailboxMessageTail tail;
    } message;

    message.tags.tag_id = tag_id;
    message.tags.buffer_size = sizeof(message.tags.data);
    message.tags.request_response_codes_and_data_size = 0;
    message.tags.data = 0;

    TRY(mailbox_send_and_receive(channel, message));
    data = message.tags.data;

    return Success;
}

char const* get_display_name_from_board_revision_id(uint32_t revision)
{
    switch (revision) {
    case 0x900021:
        return "A+";
    case 0x900032:
        return "B+";
    case 0x900092:
    case 0x920092:
    case 0x920093:
    case 0x900093:
        return "Zero";
    case 0x9000c1:
        return "Zero W";

    case 0xa01040:
    case 0xa01041:
    case 0xa02042:
    case 0xa21041:
    case 0xa22042:
        return "2B";

    case 0x9020e0:
        return "3A+";

    case 0xa22082:
    case 0xa32082:
    case 0xa52082:
    case 0xa22083:
    case 0xa02082:
        return "3B";

    case 0xa020d3:
        return "3B+";

    case 0x900061:
        return "CM";
    case 0xa020a0:
        return "CM3";

    case 0xa220a0:
    case 0xa02100:
        return "CM3+";

    case 0xa03111:
    case 0xb03111:
    case 0xb03112:
    case 0xb03114:
    case 0xc03111:
    case 0xc03112:
    case 0xc03114:
    case 0xd03114:
        return "4B";
    case 0xc03130:
        return "Pi400";
    default:
        return "Unknown";
    }
}

Error get_board_revision(uint32_t& revision)
{
    return four_bytes_message(Channel::PropertyTagsARMToVc, 0x00010002, revision);
}

Error get_firmware_revision(uint32_t& revision)
{
    return four_bytes_message(Channel::PropertyTagsARMToVc, 0x00000001, revision);
}

Error get_clock_rate(ClockId clock, uint32_t& rate)
{
    struct __attribute__((aligned(16))) {
        MailboxMessageHeader header;
        struct {
            uint32_t tag_id;
            uint32_t buffer_size;
            uint32_t request_response_codes_and_data_size;
            uint32_t data[2];
        } tags;
        MailboxMessageTail tail;
    } message;

    message.tags.tag_id = 0x00030002;
    message.tags.buffer_size = 8;
    message.tags.request_response_codes_and_data_size = 0;
    message.tags.data[0] = static_cast<uint32_t>(clock);
    message.tags.data[1] = 0;

    TRY(mailbox_send_and_receive(Channel::PropertyTagsARMToVc, message));
    rate = message.tags.data[1];

    return Success;
}

Error allocate_framebuffer(struct Framebuffer& fb)
{
    constexpr uint32_t width = 1280;
    constexpr uint32_t height = 720;
    constexpr uint32_t depth = 32;

#define TAG_HEADER        \
    uint32_t tag_id;      \
    uint32_t buffer_size; \
    uint32_t request_response_codes_and_data_size;

    struct __attribute__((aligned(16))) {
        MailboxMessageHeader header;
        struct {
            struct {
                TAG_HEADER;
                uint32_t width;
                uint32_t height;
            } set_physical_width_height_tag;
            struct {
                TAG_HEADER;
                uint32_t width;
                uint32_t height;
            } set_virtual_width_height_tag;
            struct {
                TAG_HEADER;
                uint32_t depth;
            } set_depth_tag;
            struct {
                TAG_HEADER;
                uint32_t pixel_order;
            } set_pixel_order_tag;
            struct {
                TAG_HEADER;
                uint32_t pitch;
            } get_pitch_tag;
            struct {
                TAG_HEADER;
                uint32_t x;
                uint32_t y;
            } set_virtual_offset_tag;
            struct {
                TAG_HEADER;
                uint32_t alignment_or_address;
                uint32_t size;
            } allocate_buffer_tag;
        } tags;
        MailboxMessageTail tail;
    } message;

    {
        message.tags.set_physical_width_height_tag.tag_id = 0x00048003;
        message.tags.set_physical_width_height_tag.buffer_size = 8;
        message.tags.set_physical_width_height_tag.request_response_codes_and_data_size = 0;
        message.tags.set_physical_width_height_tag.width = width;
        message.tags.set_physical_width_height_tag.height = height;
    }
    {
        message.tags.set_virtual_width_height_tag.tag_id = 0x00048004;
        message.tags.set_virtual_width_height_tag.buffer_size = 8;
        message.tags.set_virtual_width_height_tag.request_response_codes_and_data_size = 0;
        message.tags.set_virtual_width_height_tag.width = width;
        message.tags.set_virtual_width_height_tag.height = height;
    }
    {
        message.tags.set_depth_tag.tag_id = 0x00048005;
        message.tags.set_depth_tag.buffer_size = 4;
        message.tags.set_depth_tag.request_response_codes_and_data_size = 0;
        message.tags.set_depth_tag.depth = depth;
    }
    {
        message.tags.set_pixel_order_tag.tag_id = 0x00048006;
        message.tags.set_pixel_order_tag.buffer_size = 4;
        message.tags.set_pixel_order_tag.request_response_codes_and_data_size = 0;
        message.tags.set_pixel_order_tag.pixel_order = 1; // RGB
    }
    {
        message.tags.get_pitch_tag.tag_id = 0x00040008;
        message.tags.get_pitch_tag.buffer_size = 4;
        message.tags.get_pitch_tag.request_response_codes_and_data_size = 0;
    }
    {
        message.tags.set_virtual_offset_tag.tag_id = 0x00048009;
        message.tags.set_virtual_offset_tag.buffer_size = 8;
        message.tags.set_virtual_offset_tag.request_response_codes_and_data_size = 0;
        message.tags.set_virtual_offset_tag.x = 0;
        message.tags.set_virtual_offset_tag.y = 0;
    }
    {
        message.tags.allocate_buffer_tag.tag_id = 0x00040001;
        message.tags.allocate_buffer_tag.buffer_size = 8;
        message.tags.allocate_buffer_tag.request_response_codes_and_data_size = 0;
        message.tags.allocate_buffer_tag.alignment_or_address = 16;
    }

    TRY(mailbox_send_and_receive(Channel::PropertyTagsARMToVc, message));

    if (message.tags.set_physical_width_height_tag.width != width || message.tags.set_physical_width_height_tag.height != height || message.tags.set_virtual_width_height_tag.width != width || message.tags.set_virtual_width_height_tag.height != height || message.tags.set_depth_tag.depth != depth || message.tags.set_pixel_order_tag.pixel_order != 1 || message.tags.get_pitch_tag.pitch == 0 || message.tags.allocate_buffer_tag.alignment_or_address == 0)
        return Error {
            .generic_error_code = GenericErrorCode::BadResponse,
            .device_specific_error_code = message.header.request_response_code,
            .user_message = "Videocore IV refused to setup the framebuffer with the requested parameters",
            .extra_data = nullptr
        };

    fb = Framebuffer {
        .width = width,
        .height = height,
        .pitch = message.tags.get_pitch_tag.pitch,
        .depth = depth,
        .address = reinterpret_cast<uint32_t*>(message.tags.allocate_buffer_tag.alignment_or_address),
        .size = message.tags.allocate_buffer_tag.size
    };

    return Success;
}

}
