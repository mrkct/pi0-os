#include <kernel/device/keyboard.h>
#include <kernel/device/uart.h>
#include <kernel/device/miniuart.h>


namespace kernel {

klib::CircularQueue<api::KeyEvent, 32> g_keyboard_events;

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
    g_keyboard_events.push(event);
    event.press_state = false;
    g_keyboard_events.push(event);

    // When using QEMU, the terminal only sends '\r' on ENTER
    if (data == '\r') {
        notify_keyboard_event('\n');
    }
}

bool read_keyevent(api::KeyEvent &out_event)
{
    return g_keyboard_events.pop(out_event);
}

}