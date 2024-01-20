#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

void miniuart_putc(unsigned char);

extern "C" void miniuart_puts(char const* s);

void miniuart_enable_rx_irq(void (*callback)(uint8_t));

}