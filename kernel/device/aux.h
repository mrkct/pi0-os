#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

void aux_init();

void miniuart_putc(unsigned char);

extern "C" void miniuart_puts(char const* s);

void miniuart_enable_rx_irq(void (*callback)(uint8_t));


bool virtinput_is_available();

void virtinput_enable_keyboard_irq(void (*callback)(uint32_t keycode, bool pressed));

void virtinput_enable_mouse_irq(void (*callback)(int8_t rel_x, int8_t rel_y, uint16_t buttons));

}