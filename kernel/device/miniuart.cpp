#include <kernel/device/miniuart.h>
#include <kernel/device/io.h>
#include <kernel/interrupt.h>


namespace kernel {

static constexpr uint32_t AUX_IRQ = 29;

static constexpr uintptr_t AUX_BASE = bcm2835_bus_address_to_physical(0x7e215000);
static constexpr uintptr_t AUX_ENABLES = AUX_BASE + 0x04;
static constexpr uintptr_t AUX_MU_IO = AUX_BASE + 0x40;
static constexpr uintptr_t AUX_MU_IER = AUX_BASE + 0x44;
static constexpr uintptr_t AUX_MU_IIR = AUX_BASE + 0x48;
static constexpr uintptr_t AUX_MU_LSR = AUX_BASE + 0x54;
static constexpr uintptr_t AUX_MU_CNTL = AUX_BASE + 0x60;
static constexpr uintptr_t AUX_MU_STAT = AUX_BASE + 0x64;

static void (*g_rx_irq_callback)(uint8_t) = nullptr;

void miniuart_putc(unsigned char c)
{
    static constexpr uint32_t TRANSMITTER_EMPTY = 1 << 5;
    while (!(ioread32<uint32_t>(AUX_MU_LSR) & TRANSMITTER_EMPTY))
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

void miniuart_enable_rx_irq(void (*callback)(uint8_t))
{
    g_rx_irq_callback = callback;

    interrupt_install_irq1_handler(AUX_IRQ, [](auto*) {
        static constexpr uint32_t IRQ_PENDING = 1;
        if ((ioread32<uint32_t>(AUX_MU_IIR) & IRQ_PENDING) != 0)
            return;
        
        uint8_t data = ioread32<uint32_t>(AUX_MU_IO) & 0xff;
        g_rx_irq_callback(data);
    });
    
    // Check errata: TX and RX bits are swapped, and you also need to set bits 3:2
    static constexpr uint32_t RX_IRQ_ENABLE = 1 | (1 << 2);
    
    auto ier = ioread32<uint32_t>(AUX_MU_IER);
    ier |= RX_IRQ_ENABLE;
    iowrite32<uint32_t>(AUX_MU_IER, ier);
}

}
