#pragma once

#include <kernel/device/io.h>
#include <kernel/error.h>
#include <kernel/kprintf.h>
#include <stddef.h>

namespace kernel {

enum class Channel : uint32_t {
    PowerManagement,
    Framebuffer,
    VirtualUART,
    VCHIQ,
    LEDs,
    Buttons,
    TouchScreen,
    Reserved,
    PropertyTagsARMToVc,
    PropertyTagsVcToARM
};

template<uint32_t TagDataSize>
struct MailboxMessage {
    uint32_t total_size;
    uint32_t request_response_code;
    struct {
        uint32_t tag_id;
        uint32_t buffer_size;
        uint32_t request_response_codes_and_data_size;
        uint32_t data[TagDataSize];
    } tag;
    uint32_t end_tag;
} __attribute__((aligned(16)));

template<uint32_t TagDataSize>
Error mailbox_send_and_receive(Channel channel, MailboxMessage<TagDataSize> volatile& mailbox_message)
{
    static constexpr uintptr_t MAILBOX_BASE = videocore_address_to_physical(0x0000B880);

    constexpr uintptr_t MBOX_READ = MAILBOX_BASE + 0x00;
    constexpr uintptr_t MBOX_STATUS = MAILBOX_BASE + 0x18;
    constexpr uintptr_t MBOX_WRITE = MAILBOX_BASE + 0x20;

    constexpr uint32_t FULL = 1 << 31;
    while (ioread32<uint32_t>(MBOX_STATUS) & FULL)
        ;

    mailbox_message.total_size = sizeof(mailbox_message);
    mailbox_message.request_response_code = 0;
    mailbox_message.tag.buffer_size = TagDataSize;
    mailbox_message.end_tag = 0;

    iowrite32(MBOX_WRITE, static_cast<uint32_t>(channel) | (reinterpret_cast<uintptr_t>(&mailbox_message) & 0xFFFFFFF0));

    constexpr uint32_t EMPTY = 1 << 30;
    uint32_t response;
    do {
        while (ioread32<uint32_t>(MBOX_STATUS) & EMPTY)
            ;

        response = ioread32<uint32_t>(MBOX_READ);
    } while ((response & 0xf) != static_cast<uint32_t>(channel));

    if (mailbox_message.request_response_code != 0x80000000)
        return Error {
            GenericErrorCode::BadResponse,
            mailbox_message.request_response_code,
            "Mailbox response code is not 0x80000000, no further information is available",
            nullptr
        };

    return Success;
}

}
