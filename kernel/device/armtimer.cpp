#include <kernel/device/armtimer.h>
#include <kernel/device/io.h>
#include <kernel/interrupt.h>

namespace kernel {

static constexpr uintptr_t ARM_TIMER_BASE = bcm2835_bus_address_to_physical(0x7E00B000);

static constexpr uintptr_t ARM_TIMER_LOAD = ARM_TIMER_BASE + 0x400;
static constexpr uintptr_t ARM_TIMER_VALUE = ARM_TIMER_BASE + 0x404;
static constexpr uintptr_t ARM_TIMER_CONTROL = ARM_TIMER_BASE + 0x408;
static constexpr uintptr_t ARM_TIMER_IRQ_CLEAR = ARM_TIMER_BASE + 0x40c;
static constexpr uintptr_t ARM_TIMER_IRQ_RAW = ARM_TIMER_BASE + 0x410;
static constexpr uintptr_t ARM_TIMER_IRQ_MASKED = ARM_TIMER_BASE + 0x414;
static constexpr uintptr_t ARM_TIMER_RELOAD = ARM_TIMER_BASE + 0x418;
static constexpr uintptr_t ARM_TIMER_PREDIVIDER = ARM_TIMER_BASE + 0x41c;
static constexpr uintptr_t ARM_TIMER_FREE_RUNNING_COUNTER = ARM_TIMER_BASE + 0x420;

static void (*g_callback)() = nullptr;

void arm_timer_init()
{
    static constexpr uint32_t ARM_TIMER_IRQ = 0;
    interrupt_install_basic_irq_handler(ARM_TIMER_IRQ, [](auto*) {
        if (ioread32<uint32_t>(ARM_TIMER_IRQ_RAW) & (1 << ARM_TIMER_IRQ)) {
            if (g_callback != nullptr)
                g_callback();
        }
        iowrite32(ARM_TIMER_IRQ_CLEAR, 1 << ARM_TIMER_IRQ);
    });

    static constexpr uint32_t COUNTER_23BIT = 1 << 1;
    static constexpr uint32_t TIMER_INTERRUPT_ENABLED = 1 << 5;
    static constexpr uint32_t TIMER_ENABLED = 1 << 7;
    static constexpr uint32_t FREE_RUNNING_COUNTER_ENABLED = 1 << 9;

    iowrite32(ARM_TIMER_CONTROL, COUNTER_23BIT | TIMER_INTERRUPT_ENABLED | TIMER_ENABLED | FREE_RUNNING_COUNTER_ENABLED);
}

void arm_timer_exec_after(uint32_t microseconds, void (*callback)())
{
    g_callback = callback;
    iowrite32(ARM_TIMER_LOAD, microseconds);
}

}
