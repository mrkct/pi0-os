#pragma once

#include <stdint.h>
#include <stddef.h>


namespace kernel {

extern "C" void miniuart_putc(unsigned char);

extern "C" void miniuart_puts(const char *s);

}