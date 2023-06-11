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

static SystimerCallback g_repeating_callback = nullptr;
static uint32_t g_repeating_callback_interval = 0;
static SystimerCallback g_channel3_callback = nullptr;

static uint32_t ticks_plus_interval(uint32_t interval)
{
    uint32_t ticks = (uint32_t)(systimer_get_ticks() & 0xffffffff);
    if (ticks + interval < ticks)
        panic("systimer: ticks overflow");

    return ticks + interval;
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
        if (g_repeating_callback != nullptr) {
            g_repeating_callback(suspended_task_state);

            auto ticks = systimer_get_ticks();
            iowrite32<uint32_t>(SYSTIMER_C1, (uint32_t)ticks + g_repeating_callback_interval);
            iowrite32<uint32_t>(SYSTIMER_CS, CS_M1);
        }
    });
    interrupt_install_irq1_handler(3, [](auto* suspended_task_state) {
        if (g_channel3_callback != nullptr) {
            auto callback = g_channel3_callback;
            g_channel3_callback = nullptr;
            callback(suspended_task_state);
        }
        iowrite32<uint32_t>(SYSTIMER_CS, CS_M3);
    });
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

Error systimer_repeating_callback(uint32_t interval, void (*callback)(SuspendedTaskState*))
{
    if (g_repeating_callback != nullptr)
        return DeviceIsBusy;

    if (interval == 0)
        return BadParameters;

    g_repeating_callback = callback;
    g_repeating_callback_interval = interval;
    iowrite32<uint32_t>(SYSTIMER_C1, ticks_plus_interval(interval));
    return Success;
}

Error systimer_exec_after(uint64_t ticks, void (*callback)(SuspendedTaskState*))
{
    if (g_channel3_callback == nullptr) {
        g_channel3_callback = callback;
        iowrite32(SYSTIMER_C3, ticks_plus_interval(ticks));
        return Success;
    }

    return DeviceIsBusy;
}

}
