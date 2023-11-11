#include <kernel/device/keyboard.h>


namespace kernel {

klib::CircularQueue<KeyEvent, 32> g_keyboard_events;

uint32_t char_to_keycode(unsigned char c)
{
    // TODO: Implement this once we support keyboard that can send modifier keys
    return static_cast<uint32_t>(c);
}

}