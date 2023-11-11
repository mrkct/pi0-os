#include <kernel/device/gpio.h>
#include <kernel/device/io.h>
#include <kernel/interrupt.h>
#include <kernel/device/keyboard.h>
#include <kernel/device/uart.h>

namespace kernel {

static constexpr uint32_t  UART0_IRQ = 57;
static constexpr uintptr_t UART0_BASE = bcm2835_bus_address_to_physical(0x7E201000);
static constexpr uintptr_t REG_DR = UART0_BASE + 0x00;

static constexpr uintptr_t REG_RSRECR = UART0_BASE + 0x04;
static constexpr uintptr_t REG_FR = UART0_BASE + 0x18;
static constexpr uintptr_t REG_ILPR = UART0_BASE + 0x20;
static constexpr uintptr_t REG_IBRD = UART0_BASE + 0x24;
static constexpr uintptr_t REG_FBRD = UART0_BASE + 0x28;
static constexpr uintptr_t REG_LCRH = UART0_BASE + 0x2C;
static constexpr uintptr_t REG_CR = UART0_BASE + 0x30;
static constexpr uintptr_t REG_IFLS = UART0_BASE + 0x34;
static constexpr uintptr_t REG_IMSC = UART0_BASE + 0x38;
static constexpr uintptr_t REG_RIS = UART0_BASE + 0x3C;
static constexpr uintptr_t REG_MIS = UART0_BASE + 0x40;
static constexpr uintptr_t REG_ICR = UART0_BASE + 0x44;
static constexpr uintptr_t REG_DMACR = UART0_BASE + 0x48;
static constexpr uintptr_t REG_ITCR = UART0_BASE + 0x80;
static constexpr uintptr_t REG_ITIP = UART0_BASE + 0x84;
static constexpr uintptr_t REG_ITOP = UART0_BASE + 0x88;
static constexpr uintptr_t REG_TDR = UART0_BASE + 0x8C;

Error uart_init(void*);
Error uart_read(void*, uint8_t&);
Error uart_write(void*, uint8_t);
static void notify_keyboard_event(uint8_t data);

struct UartData {
    bool initialized { false };
};
static UartData g_uart0;

CharacterDevice uart_device()
{
    return { .init = uart_init, .read = uart_read, .write = uart_write, .data = &g_uart0 };
}

Error uart_init(void* data)
{
    UartData* uart_data = static_cast<UartData*>(data);
    if (uart_data->initialized)
        return Success;

    // 1. Disable UART0
    iowrite32<uint32_t>(REG_CR, 0x00000000);

    // 2. Setup the GPIO pins 14 and 15 by disabling pull-up/down and
    //    setting them to alternate function 0
    gpio_set_pin_function(14, PinFunction::Alternate0);
    gpio_set_pin_function(15, PinFunction::Alternate0);
    gpio_set_pin_pull_up_down_state(14, PullUpDownState::Disable);
    gpio_set_pin_pull_up_down_state(15, PullUpDownState::Disable);

    // 3. Clear pending interrupts
    iowrite32<uint32_t>(REG_ICR, 0x7FF);

    // 4. Set integer & fractional part of baud rate (note: hardcoded to 115200)
    //    Divider = UART_CLOCK / (16 * Baud)
    //    Fraction part register = (Fractional part * 64) + 0.5
    //    UART_CLOCK = 3000000; Baud = 115200

    iowrite32<uint32_t>(REG_IBRD, 1);
    iowrite32<uint32_t>(REG_FBRD, 40);

    // 5. Set line characteristics (8 bits, 1 stop bit, no parity)
    static constexpr uint32_t WORD_LENGTH_EIGHT_BITS = 0b11 << 5;
    static constexpr uint32_t FIFO_ENABLED = 1 << 4;
    iowrite32<uint32_t>(REG_LCRH, FIFO_ENABLED | WORD_LENGTH_EIGHT_BITS);

    // 6. Enable the "receive" interrupt
    static constexpr uint32_t RECEIVE_IRQ_MASK = 1 << 4;
    iowrite32<uint32_t>(REG_IMSC, RECEIVE_IRQ_MASK);

    // 7. Enable UART0, receive & transfer part of UART
    static constexpr uint32_t ENABLE_UART0 = 1 << 0;
    static constexpr uint32_t ENABLE_TRANSMIT = 1 << 8;
    static constexpr uint32_t ENABLE_RECEIVE = 1 << 9;
    iowrite32<uint32_t>(REG_CR, ENABLE_UART0 | ENABLE_TRANSMIT | ENABLE_RECEIVE);

    interrupt_install_irq2_handler(UART0_IRQ - 32, [](auto*) {
        uint8_t data = ioread32<uint32_t>(REG_DR) & 0xff;
        static constexpr uint32_t RECEIVE_IRQ_CLEAR = 1 << 4;
        iowrite32(REG_ICR, RECEIVE_IRQ_CLEAR);

        notify_keyboard_event(data);
    });

    uart_data->initialized = true;

    return Success;
}

Error uart_read(void* data, uint8_t& c)
{
    auto* uart_data = static_cast<UartData*>(data);
    if (!uart_data->initialized && false)
        return DeviceNotInitialized;

    static constexpr uint32_t RECEIVE_FIFO_EMPTY = 1 << 4;
    while (ioread32<uint32_t>(REG_FR) & RECEIVE_FIFO_EMPTY)
        ;
    c = ioread32<uint32_t>(REG_DR) & 0xFF;

    return Success;
}

Error uart_write(void* data, uint8_t c)
{
    auto* uart_data = static_cast<UartData*>(data);
    if (!uart_data->initialized && false)
        return DeviceNotInitialized;

    static constexpr uint32_t TRANSMIT_FIFO_FULL = 1 << 5;
    while (ioread32<uint32_t>(REG_FR) & TRANSMIT_FIFO_FULL)
        ;
    iowrite32<uint32_t>(REG_DR, c);

    return Success;
}

static void notify_keyboard_event(uint8_t data)
{
    KeyEvent event = {
        .character = data,
        .keycode = char_to_keycode(data),
        .press_state = true
    };
    g_keyboard_events.push(event);
    event.press_state = false;
    g_keyboard_events.push(event);
}

}
