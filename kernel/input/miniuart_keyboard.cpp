#include "miniuart_keyboard.h"
#include <api/input.h>


namespace kernel {

static void uart_irq(uint8_t data)
{
    uint32_t keycode = char_to_keycode(data);
    notify_key_event(api::KeyEvent {
        .character = data,
        .keycode = keycode,
        .pressed = true,
        .raw_character = true
    });
    notify_key_event(api::KeyEvent {
        .character = data,
        .keycode = keycode,
        .pressed = false,
        .raw_character = false
    });

    if (data == '\r') {
        notify_key_event(api::KeyEvent {
            .character = '\n',
            .keycode = api::KEYCODE_RET,
            .pressed = true,
            .raw_character = true
        });
        notify_key_event(api::KeyEvent {
            .character = '\n',
            .keycode = api::KEYCODE_RET,
            .pressed = false,
            .raw_character = false
        });
    }
}

void miniuart_keyboard_init()
{
    miniuart_enable_rx_irq(uart_irq);    
}

}