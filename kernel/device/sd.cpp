#include <kernel/device/gpio.h>
#include <kernel/device/io.h>
#include <kernel/device/sd.h>
#include <kernel/kprintf.h>

namespace kernel {

namespace regs {

constexpr uintptr_t SDHC_BASE = bcm2835_bus_address_to_physical(0x7E300000);

constexpr uintptr_t ARGUMENT_2 = SDHC_BASE + 0x00;
constexpr uintptr_t BLOCK_SIZE_AND_BLOCK_COUNT = SDHC_BASE + 0x04;
constexpr uintptr_t ARGUMENT_1 = SDHC_BASE + 0x08;
constexpr uintptr_t TRANSFER_MODE_AND_COMMAND = SDHC_BASE + 0x0c;
constexpr uintptr_t RESPONSE_0 = SDHC_BASE + 0x10;
constexpr uintptr_t RESPONSE_1 = SDHC_BASE + 0x14;
constexpr uintptr_t RESPONSE_2 = SDHC_BASE + 0x18;
constexpr uintptr_t RESPONSE_3 = SDHC_BASE + 0x1c;
constexpr uintptr_t BUFFER_DATA_PORT = SDHC_BASE + 0x20;
constexpr uintptr_t PRESENT_STATE = SDHC_BASE + 0x24;
constexpr uintptr_t HOST_CONFIGURATION_0 = SDHC_BASE + 0x28;
constexpr uintptr_t HOST_CONFIGURATION_1 = SDHC_BASE + 0x2c;
constexpr uintptr_t INTERRUPT_STATUS = SDHC_BASE + 0x30;
constexpr uintptr_t INTERRUPT_STATUS_ENABLE = SDHC_BASE + 0x34;
constexpr uintptr_t INTERRUPT_SIGNAL_ENABLE = SDHC_BASE + 0x38;
constexpr uintptr_t HOST_CONFIGURATION_2 = SDHC_BASE + 0x3c;
constexpr uintptr_t CAPABILITIES = SDHC_BASE + 0x40;
constexpr uintptr_t MAXIMUM_CURRENT_CAPABILITIES = SDHC_BASE + 0x48;
constexpr uintptr_t FORCE_EVENT_FOR_AUTO_CMD_ERROR_STATUS = SDHC_BASE + 0x50;
constexpr uintptr_t ADMA_ERROR_STATUS = SDHC_BASE + 0x54;
constexpr uintptr_t ADMA_SYSTEM_ADDRESS = SDHC_BASE + 0x58;
constexpr uintptr_t BOOT_TIMEOUT_CONTROL_AND_STATUS = SDHC_BASE + 0x60;
constexpr uintptr_t DEBUG_SELECTION = SDHC_BASE + 0x64;
constexpr uintptr_t SHARED_BUS_CONTROL = SDHC_BASE + 0x70;
constexpr uintptr_t SLOT_INTERRUPT_STATUS_AND_VERSION = SDHC_BASE + 0xfc;

};

// In "Host Configuration 1"
static constexpr uint32_t INTERNAL_CLOCK_ENABLE = 1 << 0;
static constexpr uint32_t INTERNAL_CLOCK_STABLE = 1 << 1;
static constexpr uint32_t SD_CLOCK_ENABLE = 1 << 2;

// In "Interrupt Status"
static constexpr uint32_t COMMAND_COMPLETE = 1 << 0;
static constexpr uint32_t TRANSFER_COMPLETE = 1 << 1;
static constexpr uint32_t BUFFER_WRITE_READY = 1 << 4;
static constexpr uint32_t BUFFER_READ_READY = 1 << 5;

// In "Present State"
static constexpr uint32_t COMMAND_INHIBIT = 1 << 1;

// In "OCR"
static constexpr uint32_t CARD_CAPACITY_STATUS = 1 << 30;

enum class Command : uint32_t {
    GoIdleState = 0x00000000,
    AllSendCID = 0x02010000,
    SendRelativeAddr = 0x03020000,
    SelectCard = 0x07030000,
    SendIfCond = 0x08020000,
    SendCSD = 0x9090000,
    SendCID = 0x2090000,
    SetBlockLen = 0x100a0000,
    ReadSingleBlock = 0x11220010,
    ReadMultipleBlock = 0x12220032,
    WriteBlock = 0x182a0000,
    WriteMultipleBlock = 0x192a0026,
    AppCmd = 0x37000000,
};

enum class AppCommand : uint32_t {
    SetBusWidth = 0x60a0000,
    SDSendOpCond = 0x29020000,
    SendSCR = 0x33220010,
};

template<typename Callback>
Error retry_with_timeout(Callback callback)
{
    constexpr uint32_t TIMEOUT = 1000000;
    for (uint32_t i = 0; i < TIMEOUT; ++i) {
        if (callback())
            return Success;

        wait_cycles(500);
    }

    return ResponseTimeout;
}

enum class Version {
    V1,
    V2,
    V3,
    V4,
    Unknown
};

static Version sdhc_version()
{
    uint32_t version = 0xff & (ioread32<uint32_t>(regs::SLOT_INTERRUPT_STATUS_AND_VERSION) >> 16);
    switch (version) {
    case 0:
        return Version::V1;
    case 1:
        return Version::V2;
    case 2:
        return Version::V3;
    case 3:
        return Version::V4;
    default:
        return Version::Unknown;
    }
}

static Error sdhc_reset()
{
    constexpr uint32_t SOFTWARE_RESET_FOR_ALL = 1 << 24;

    for (int i = 21; i <= 27; i++)
        TRY(gpio_set_pin_function(i, PinFunction::Alternate3));
    TRY(gpio_set_pin_high_detect_enable(21, true));

    iowrite32(regs::HOST_CONFIGURATION_0, 0);
    iowrite32(regs::HOST_CONFIGURATION_1, ioread32<uint32_t>(regs::HOST_CONFIGURATION_1) | SOFTWARE_RESET_FOR_ALL);

    TRY(retry_with_timeout([] { return (ioread32<uint32_t>(regs::HOST_CONFIGURATION_1) & SOFTWARE_RESET_FOR_ALL) == 0; }));

    return Success;
}

static void sd_clock_stop()
{
    auto r = ioread32<uint32_t>(regs::HOST_CONFIGURATION_1);
    r &= ~SD_CLOCK_ENABLE;
    iowrite32<uint32_t>(regs::HOST_CONFIGURATION_1, r);
}

static uint32_t retrieve_sd_clock_frequency()
{
    constexpr auto ONE_MHZ = 1'000'000;
    return ONE_MHZ * ioread32<uint32_t>(regs::CAPABILITIES) >> 8 & 0xff;
}

static uint32_t calculate_sd_clock_divisor(uint32_t base_sd_clock_frequency, uint32_t target_frequency)
{
    for (uint32_t divisor = 1; divisor <= 256; divisor *= 2) {
        if (base_sd_clock_frequency / divisor <= target_frequency)
            return divisor >> 1;
    }

    return 256;
}

static Error sd_clock_supply(uint32_t target_frequency)
{
    uint32_t base_sd_clock_frequency = retrieve_sd_clock_frequency();
    uint32_t divisor = calculate_sd_clock_divisor(base_sd_clock_frequency, target_frequency);

    uint32_t eight_lower_bits_of_sdclk_frequency_select = (divisor & 0xff) << 8;
    uint32_t sdclk_frequency_select = eight_lower_bits_of_sdclk_frequency_select;
    if (sdhc_version() == Version::V3 || sdhc_version() == Version::V4) {
        uint32_t two_upper_bits_of_sdclk_frequency_select = (divisor >> 8 & 0x3) << 6;
        sdclk_frequency_select |= two_upper_bits_of_sdclk_frequency_select;
    }
    iowrite32(regs::HOST_CONFIGURATION_1, ioread32<uint32_t>(regs::HOST_CONFIGURATION_1) | INTERNAL_CLOCK_ENABLE | sdclk_frequency_select);

    TRY(retry_with_timeout([] { return (ioread32<uint32_t>(regs::HOST_CONFIGURATION_1) & INTERNAL_CLOCK_STABLE) != 0; }));

    iowrite32(regs::HOST_CONFIGURATION_1, ioread32<uint32_t>(regs::HOST_CONFIGURATION_1) | SD_CLOCK_ENABLE);

    return Success;
}

static bool is_sd_clock_enabled()
{
    return ioread32<uint32_t>(regs::HOST_CONFIGURATION_1) & SD_CLOCK_ENABLE;
}

static Error send_command(uint32_t cmd, uint32_t arg, uint32_t response[4])
{
    TRY(retry_with_timeout([&]() { return !(ioread32<uint32_t>(regs::PRESENT_STATE) & COMMAND_INHIBIT); }));

    iowrite32(regs::ARGUMENT_1, arg);
    iowrite32(regs::TRANSFER_MODE_AND_COMMAND, cmd);

    TRY(retry_with_timeout([&]() { return ioread32<uint32_t>(regs::INTERRUPT_STATUS) & COMMAND_COMPLETE; }));
    iowrite32(regs::INTERRUPT_STATUS, COMMAND_COMPLETE);

    response[0] = ioread32<uint32_t>(regs::RESPONSE_0);
    response[1] = ioread32<uint32_t>(regs::RESPONSE_1);
    response[2] = ioread32<uint32_t>(regs::RESPONSE_2);
    response[3] = ioread32<uint32_t>(regs::RESPONSE_3);

    return Success;
}

static Error send_command(Command cmd, uint32_t arg, uint32_t response[4])
{
    return send_command(static_cast<uint32_t>(cmd), arg, response);
}

static Error send_app_command(AppCommand cmd, uint32_t rca, uint32_t arg, uint32_t response[4])
{
    TRY(send_command(Command::AppCmd, rca, response));
    return send_command(static_cast<uint32_t>(cmd), arg, response);
}

Error sdhc_init()
{
    sdhc_reset();

    iowrite32(regs::INTERRUPT_STATUS_ENABLE, ~0);

    return Success;
}

bool sdhc_contains_card()
{
    constexpr uint32_t CARD_INSERTED = 1 << 16;
    return ioread32<uint32_t>(regs::PRESENT_STATE) & CARD_INSERTED;
}

Error sdhc_initialize_inserted_card(SDCard& sd)
{
    if (!sdhc_contains_card())
        return DeviceNotConnected;

    if (is_sd_clock_enabled())
        sd_clock_stop();
    TRY(sd_clock_supply(400000));

    uint32_t response[4];

    TRY(send_command(Command::GoIdleState, 0, response));

    TRY(send_command(Command::SendIfCond, 0x1aa, response));
    if (response[0] != 0x1aa)
        return Error {
            .generic_error_code = GenericErrorCode::BadResponse,
            .device_specific_error_code = response[0],
            .user_message = "SDHC did not accept the proposed voltage range",
            .extra_data = nullptr
        };

    TRY(retry_with_timeout([&] {
        static constexpr uint32_t CARD_POWER_UP_STATUS = 1 << 31;

        if (auto e = send_app_command(AppCommand::SDSendOpCond, 0, 0x50ff8000, response); !e.is_success())
            return false;

        sd.ocr = response[0];
        return (sd.ocr & CARD_POWER_UP_STATUS) != 0;
    }));

    TRY(send_command(Command::AllSendCID, 0, sd.cid));

    TRY(send_command(Command::SendRelativeAddr, 0, response));
    sd.rca = response[0];

    TRY(send_command(Command::SendCSD, sd.rca, sd.csd));

    TRY(send_command(Command::SelectCard, sd.rca, response));

    if (!(sd.ocr & CARD_CAPACITY_STATUS)) {
        TRY(send_command(Command::SetBlockLen, 512, response));
    }

    TRY(send_app_command(AppCommand::SetBusWidth, sd.rca, 2, response));

    sd.is_initialized = true;

    return Success;
}

Error sd_read_block(SDCard& card, uint32_t block_idx, uint32_t block_count, uint8_t* datau8)
{
    if (!card.is_initialized)
        return DeviceNotInitialized;

    uint32_t* buffer = reinterpret_cast<uint32_t*>(datau8);
    auto cmd = block_count == 1 ? Command::ReadSingleBlock : Command::ReadMultipleBlock;

    iowrite32<uint32_t>(regs::BLOCK_SIZE_AND_BLOCK_COUNT, (block_count << 16) | 512);

    uint32_t block_addr = block_idx;
    if (!(card.ocr & CARD_CAPACITY_STATUS)) {
        // CCS=0 means the block address is byte addressed, not block addressed
        block_addr *= 512;
    }
    iowrite32<uint32_t>(regs::ARGUMENT_1, block_addr);
    iowrite32<uint32_t>(regs::TRANSFER_MODE_AND_COMMAND, static_cast<uint32_t>(cmd));

    TRY(retry_with_timeout([] { return ioread32<uint32_t>(regs::INTERRUPT_STATUS) & COMMAND_COMPLETE; }));

    for (size_t i = 0; i < block_count; i++) {
        TRY(retry_with_timeout([] { return ioread32<uint32_t>(regs::INTERRUPT_STATUS) & BUFFER_READ_READY; }));

        iowrite32<uint32_t>(regs::INTERRUPT_STATUS, BUFFER_READ_READY);

        for (size_t j = 0; j < 128; j++)
            buffer[i * 128 + j] = ioread32<uint32_t>(regs::BUFFER_DATA_PORT);
    }

    TRY(retry_with_timeout([] { return ioread32<uint32_t>(regs::INTERRUPT_STATUS) & TRANSFER_COMPLETE; }));
    iowrite32<uint32_t>(regs::INTERRUPT_STATUS, TRANSFER_COMPLETE);

    return Success;
}

}
