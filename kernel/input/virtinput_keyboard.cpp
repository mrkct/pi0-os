#include "virtinput_keyboard.h"

namespace kernel {

Error virtinput_keyboard_init(void)
{
    if (!virtinput_is_available()) {
        return NotSupported;
    }

    virtinput_enable_keyboard_irq([](auto keycode, auto pressed) {
        notify_key_event(api::KeyEvent {
            .character = 0,
            .keycode = keycode,
            .pressed = pressed,
            .raw_character = false
        });
    });

    return Success;
}

}