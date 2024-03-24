#pragma once

#include <api/syscalls.h>
#include <kernel/lib/circular_queue.h>

namespace kernel {

enum class KeyboardSource {
    MiniUart,
    Uart0
};

uint32_t char_to_keycode(unsigned char);

void init_user_keyboard(KeyboardSource);

bool read_keyevent(api::KeyEvent&);

}