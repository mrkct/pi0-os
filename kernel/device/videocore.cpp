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
    __attribute__((aligned(16))) MailboxMessage<1> message = MailboxMessage<1> {
        .total_size = sizeof(message),
        .request_response_code = 0,
        .tag = {
            .tag_id = tag_id,
            .buffer_size = 4,
            .request_response_codes_and_data_size = 0,
            .data = { 0 } },
        .end_tag = 0
    };

    auto r = mailbox_send_and_receive(channel, message);
    if (r.is_success())
        data = message.tag.data[0];

    return r;
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
    __attribute__((aligned(16))) MailboxMessage<2> message = MailboxMessage<2> {
        .total_size = sizeof(message),
        .request_response_code = 0,
        .tag = {
            .tag_id = 0x00030002,
            .buffer_size = 8,
            .request_response_codes_and_data_size = 0,
            .data = { static_cast<uint32_t>(clock), 0 } },
        .end_tag = 0
    };

    auto r = mailbox_send_and_receive(Channel::PropertyTagsARMToVc, message);
    if (r.is_success())
        rate = message.tag.data[1];

    return r;
}

}
