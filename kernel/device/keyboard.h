#pragma once

#include <api/syscalls.h>
#include <kernel/lib/circular_queue.h>


namespace kernel {

extern klib::CircularQueue<KeyEvent, 32> g_keyboard_events;

uint32_t char_to_keycode(unsigned char );

}