#include <kernel/device/keyboard.h>
#include <kernel/device/uart.h>
#include <kernel/device/miniuart.h>


namespace kernel {

static void (*g_cb)(api::KeyEvent const&) = nullptr;

static void notify_keyboard_event(uint8_t data);

uint32_t char_to_keycode(unsigned char c)
{
    // TODO: Implement this once we support keyboard that can send modifier keys
    return static_cast<uint32_t>(c);
}

void init_user_keyboard(KeyboardSource source)
{
    switch (source) {
    case KeyboardSource::MiniUart:
        miniuart_enable_rx_irq(notify_keyboard_event);
        break;
    case KeyboardSource::Uart0:
        uart_enable_rx_irq(notify_keyboard_event);
        break;
    }
}

static void notify_keyboard_event(uint8_t data)
{
    api::KeyEvent event = {
        .character = data,
        .keycode = char_to_keycode(data),
        .press_state = true
    };
    if (g_cb)
        g_cb(event);

    event.press_state = false;
    if (g_cb)
        g_cb(event);

    // When using QEMU, the terminal only sends '\r' on ENTER
    if (data == '\r') {
        notify_keyboard_event('\n');
    }
}

void set_keyboard_event_listener(void (*cb)(api::KeyEvent const&))
{
    g_cb = cb;
}

}