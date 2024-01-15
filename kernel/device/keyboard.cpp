#include <kernel/device/keyboard.h>
#include <kernel/device/uart.h>
#include <kernel/device/miniuart.h>


namespace kernel {

klib::CircularQueue<KeyEvent, 32> g_keyboard_events;

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