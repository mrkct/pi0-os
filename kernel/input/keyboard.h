#pragma once

#include <api/syscalls.h>
#include <kernel/lib/circular_queue.h>

namespace kernel {

void notify_key_event(api::KeyEvent update);

uint32_t char_to_keycode(unsigned char);

typedef void (*KeyboardEventListener)(api::KeyEvent const&);

void set_keyboard_event_listener(KeyboardEventListener cb);

}