#include <kernel/device/io.h>
#include <kernel/device/systimer.h>
#include <kernel/interrupt.h>

namespace kernel {

static constexpr uintptr_t SYSTIMER_BASE = bcm2835_bus_address_to_physical(0x7E003000);

static constexpr uintptr_t SYSTIMER_CS = SYSTIMER_BASE + 0x00;
static constexpr uintptr_t SYSTIMER_CLO = SYSTIMER_BASE + 0x04;
static constexpr uintptr_t SYSTIMER_CHI = SYSTIMER_BASE + 0x08;
static constexpr uintptr_t SYSTIMER_C1 = SYSTIMER_BASE + 0x10;
static constexpr uintptr_t SYSTIMER_C3 = SYSTIMER_BASE + 0x18;

static SystimerCallback g_channel1_callback = nullptr;
static SystimerCallback g_channel3_callback = nullptr;

static uint64_t g_ticks_per_millisecond = 0;

int64_t systimer_ms_to_ticks(uint32_t ms)
{
    kassert(g_ticks_per_millisecond != 0);
    return ms * g_ticks_per_millisecond;
}

void systimer_init()
{
    /*
        NOTE: The BCM2835 ARM Peripherals manual is incomplete.
        Consider these facts:
        - Channels 0 and 2 are reserved for GPU usage and we should not
          touch them from the ARM side
        - Channels 1 and 3 are available for ARM usage and their IRQ numbers
          are 1 and 3 of the IRQ Basic Pending 1 register

        These facts come from this site:
        https://xinu.cs.mu.edu/index.php/BCM2835_Interrupt_Controller
    */
    static constexpr uint32_t CS_M1 = 1 << 1;
    static constexpr uint32_t CS_M3 = 1 << 3;

    iowrite32<uint32_t>(SYSTIMER_CS, CS_M1 | CS_M3);

    interrupt_install_irq1_handler(1, [](auto* suspended_task_state) {
        iowrite32<uint32_t>(SYSTIMER_CS, CS_M1);
        if (g_channel1_callback != nullptr)
            g_channel1_callback(suspended_task_state);
    });
    interrupt_install_irq1_handler(3, [](auto* suspended_task_state) {
        iowrite32<uint32_t>(SYSTIMER_CS, CS_M3);
        if (g_channel3_callback != nullptr)
            g_channel3_callback(suspended_task_state);
    });

    // FIXME: This is not correct. We should read the clock frequency from the clock tree
    g_ticks_per_millisecond = 1000;
}

uint64_t systimer_get_ticks()
{
    uint64_t high, low;

    do {
        high = ioread32<uint32_t>(SYSTIMER_CHI);
        low = ioread32<uint32_t>(SYSTIMER_CLO);
        if (high == ioread32<uint32_t>(SYSTIMER_CHI))
            return (high << 32) | low;
    } while (true);
}

Error systimer_install_handler(SystimerChannel channel, SystimerCallback callback)
{
    switch (channel) {
    case SystimerChannel::Channel1:
        if (g_channel1_callback == nullptr) {
            g_channel1_callback = callback;
            return Success;
        }
        return DeviceIsBusy;
    case SystimerChannel::Channel3:
        if (g_channel3_callback == nullptr) {
            g_channel3_callback = callback;
            return Success;
        }
        return DeviceIsBusy;
    default:
        kassert_not_reached();
    }
}

Error systimer_trigger(SystimerChannel channel, uint32_t ticks)
{
    uint32_t current_ticks = ioread32<uint32_t>(SYSTIMER_CLO);

    switch (channel) {
    case SystimerChannel::Channel1:
        iowrite32<uint32_t>(SYSTIMER_C1, current_ticks + ticks);
        return Success;
    case SystimerChannel::Channel3:
        iowrite32<uint32_t>(SYSTIMER_C3, current_ticks + ticks);
        return Success;
    default:
        kassert_not_reached();
    }
}

}
