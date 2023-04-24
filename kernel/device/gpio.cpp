#include <kernel/device/gpio.h>
#include <kernel/device/io.h>

namespace kernel {

static constexpr uintptr_t GPIO_BASE = bcm2835_bus_address_to_physical(0x7e200000);

static constexpr uintptr_t GPFSEL0 = GPIO_BASE + 0x00;
static constexpr uintptr_t GPFSEL1 = GPIO_BASE + 0x04;
static constexpr uintptr_t GPFSEL2 = GPIO_BASE + 0x08;
static constexpr uintptr_t GPFSEL3 = GPIO_BASE + 0x0c;
static constexpr uintptr_t GPFSEL4 = GPIO_BASE + 0x10;
static constexpr uintptr_t GPFSEL5 = GPIO_BASE + 0x14;
static constexpr uintptr_t GPFSEL_REGS[] = { GPFSEL0, GPFSEL1, GPFSEL2, GPFSEL3, GPFSEL4, GPFSEL5 };

static constexpr uintptr_t GPHEN0 = GPIO_BASE + 0x64;
static constexpr uintptr_t GPHEN1 = GPIO_BASE + 0x68;

static constexpr uintptr_t GPPUD = GPIO_BASE + 0x94;
static constexpr uintptr_t GPPUDCLK0 = GPIO_BASE + 0x98;
static constexpr uintptr_t GPPUDCLK1 = GPIO_BASE + 0x9c;

Error gpio_set_pin_function(uint8_t pin, PinFunction f)
{
    if (pin > 53)
        return BadParameters;

    auto reg = GPFSEL_REGS[pin / 10];

    auto r = ioread32<uint32_t>(reg);
    r &= ~(0b111 << ((pin % 10) * 3));
    r |= static_cast<uint32_t>(f) << ((pin % 10) * 3);
    iowrite32(reg, r);

    return Success;
}

Error gpio_set_pin_pull_up_down_state(uint8_t pin, PullUpDownState state)
{
    if (pin > 53)
        return BadParameters;

    iowrite32(GPPUD, static_cast<uint32_t>(state));

    wait_cycles(150);

    if (pin < 32) {
        iowrite32(GPPUDCLK0, 1 << pin);
    } else {
        iowrite32(GPPUDCLK1, 1 << (pin - 32));
    }

    wait_cycles(150);

    iowrite32(GPPUD, 0);
    iowrite32(GPPUDCLK0, 0);
    iowrite32(GPPUDCLK1, 0);

    return Success;
}

Error gpio_set_pin_high_detect_enable(uint8_t pin, bool enable)
{
    if (pin > 53)
        return BadParameters;

    auto reg = pin < 32 ? GPHEN0 : GPHEN1;
    if (enable)
        iowrite32(reg, ioread32<uint32_t>(reg) | (1 << (pin % 32)));
    else
        iowrite32(reg, ioread32<uint32_t>(reg) & ~(1 << (pin % 32)));

    return Success;
}

}
