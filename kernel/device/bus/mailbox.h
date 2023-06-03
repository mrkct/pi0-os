#pragma once

#include <kernel/device/io.h>
#include <kernel/error.h>
#include <kernel/kprintf.h>
#include <kernel/memory/vm.h>
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

struct MailboxMessageHeader {
    uint32_t total_size;
    uint32_t request_response_code;
};

struct MailboxMessageTail {
    uint32_t end_tag;
};

template<typename T>
concept MailboxMessage = requires(T message) {
                             {
                                 message.header
                             };
                             {
                                 message.tags
                             };
                             {
                                 message.tail
                             };
                         };

template<MailboxMessage M>
Error mailbox_send_and_receive(Channel channel, M volatile& message)
{
    static constexpr uintptr_t MAILBOX_BASE = videocore_address_to_physical(0x0000B880);

    constexpr uintptr_t MBOX_READ = MAILBOX_BASE + 0x00;
    constexpr uintptr_t MBOX_STATUS = MAILBOX_BASE + 0x18;
    constexpr uintptr_t MBOX_WRITE = MAILBOX_BASE + 0x20;

    constexpr uint32_t FULL = 1 << 31;
    while (ioread32<uint32_t>(MBOX_STATUS) & FULL)
        ;

    message.header.total_size = sizeof(message);
    message.header.request_response_code = 0;
    message.tail.end_tag = 0;

    iowrite32(MBOX_WRITE, static_cast<uint32_t>(channel) | (virt2phys(reinterpret_cast<uintptr_t>(&message)) & 0xFFFFFFF0));

    constexpr uint32_t EMPTY = 1 << 30;
    uint32_t response;
    do {
        while (ioread32<uint32_t>(MBOX_STATUS) & EMPTY)
            ;

        response = ioread32<uint32_t>(MBOX_READ);
    } while ((response & 0xf) != static_cast<uint32_t>(channel));

    if (message.header.request_response_code != 0x80000000)
        return Error {
            GenericErrorCode::BadResponse,
            message.header.request_response_code,
            "Mailbox response code is not 0x80000000, no further information is available",
            nullptr
        };

    return Success;
}

}
