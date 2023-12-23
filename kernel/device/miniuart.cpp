#include <kernel/device/miniuart.h>
#include <kernel/device/io.h>


namespace kernel {

static constexpr uintptr_t AUX_BASE = bcm2835_bus_address_to_physical(0x7e215000);
static constexpr uintptr_t AUX_ENABLES = AUX_BASE + 0x04;
static constexpr uintptr_t AUX_MU_IO = AUX_BASE + 0x40;
static constexpr uintptr_t AUX_MU_CNTL = AUX_BASE + 0x60;
static constexpr uintptr_t AUX_MU_STAT = AUX_BASE + 0x64;

extern "C" void miniuart_putc(unsigned char c)
{
    static constexpr uint32_t TRANSMITTER_DONE = 1 << 9;
    while (!(ioread32<uint32_t>(AUX_MU_STAT) & TRANSMITTER_DONE))
		;
	
	iowrite32<uint32_t>(AUX_MU_IO, c);
}

extern "C" void miniuart_puts(const char *s)
{
    while (*s) {
        miniuart_putc(*s);
        s++;
    }
    miniuart_putc('\r');
    miniuart_putc('\n');
}

}
