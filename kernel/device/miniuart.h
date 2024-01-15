#pragma once

#include <stdint.h>
#include <stddef.h>


namespace kernel {

void miniuart_putc(unsigned char);

extern "C" void miniuart_puts(const char *s);

void miniuart_enable_rx_irq(void (*callback)(uint8_t));

}