#include "bcm2835_gpio.h"


BCM2835GPIOController::BCM2835GPIOController(Config const *config)
    : m_iobase(config->iobase), m_offset(config->offset)
{
}

int32_t BCM2835GPIOController::init_for_early_boot()
{
    if (r != nullptr)
        return 0;
    
    r = static_cast<RegisterMap volatile*>(ioremap(m_iobase + m_offset, sizeof(RegisterMap)));
    return 0;
}

int32_t BCM2835GPIOController::init()
{
    return init_for_early_boot();
}

int32_t BCM2835GPIOController::shutdown()
{
    todo();
    return 0;
}

int32_t BCM2835GPIOController::configure_pin(uint32_t port, uint32_t pin, PinFunction function)
{
    if (port != 0 || pin >= PIN_COUNT)
        return -EINVAL;

    uint32_t function_bitmask = 0;
    switch (function) {
    case PinFunction::Input:
        function_bitmask = 0b000;
        break;
    case PinFunction::Output:
        function_bitmask = 0b001;
        break;
    case PinFunction::Alt0:
        function_bitmask = 0b100;
        break;
    case PinFunction::Alt1:
        function_bitmask = 0b101;
        break;
    case PinFunction::Alt2:
        function_bitmask = 0b110;
        break;
    case PinFunction::Alt3:
        function_bitmask = 0b111;
        break;
    case PinFunction::Alt4:
        function_bitmask = 0b011;
        break;
    case PinFunction::Alt5:
        function_bitmask = 0b010;
        break;
    }

    auto pinselect = ioread32(&r->GPFSEL[pin / 10]);
    pinselect &= ~(0b111 << ((pin % 10) * 3));
    pinselect |= function_bitmask << ((pin % 10) * 3);
    iowrite32(&r->GPFSEL[pin / 10], pinselect);

    return 0;
}

int32_t BCM2835GPIOController::configure_pin_pull_up_down(uint32_t port, uint32_t pin, PullState state)
{
    if (port != 0 || pin >= PIN_COUNT)
        return -EINVAL;

    uint32_t pull_up_down_bitmask = 0;
    switch (state) {
    case PullState::None:
        pull_up_down_bitmask = 0b00;
        break;
    case PullState::Down:
        pull_up_down_bitmask = 0b01;
        break;
    case PullState::Up:
        pull_up_down_bitmask = 0b10;
        break;
    }

    iowrite32(&r->GPPUD, pull_up_down_bitmask);
    wait_cycles(150);
    if (pin < 32) {
        iowrite32(&r->GPPUDCLK[0], 1 << pin);
        wait_cycles(150);
        iowrite32(&r->GPPUDCLK[0], 0);
    } else {
        iowrite32(&r->GPPUDCLK[1], 1 << (pin - 32));
        wait_cycles(150);
        iowrite32(&r->GPPUDCLK[1], 0);
    }

    return 0;
}


int32_t BCM2835GPIOController::get_pin_state(uint32_t, uint32_t)
{
    todo();
    return 0;
}

int32_t BCM2835GPIOController::set_pin_state(uint32_t, uint32_t, PinState)
{
    todo();
    return 0;
}
