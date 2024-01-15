#include <kernel/device/chardev.h>
#include <kernel/device/io.h>

namespace kernel {

CharacterDevice uart_device();

void uart_enable_rx_irq(void (*callback)(uint8_t));

}
