#include "pl011.h"


static constexpr uint32_t RXINTR_MASK = 1 << 4;

// In "Flag register, UARTFR"
// RXFE: Receive FIFO Empty
static constexpr uint32_t RXFE_MASK = 1 << 4;

PL011UART::PL011UART(Config const *config)
    :   UART(), m_config(*config)
{
}

int32_t PL011UART::init_except_interrupt()
{
    if (r != nullptr)
        return 0;

    r = static_cast<RegisterMap volatile*>(ioremap(m_config.physaddr, sizeof(RegisterMap)));

    iowrite32(&r->CR, 0x0);         // Turn off UART
    iowrite32(&r->ICR, 0x7ff);      // Clear all IRQs

    // 4. Set integer & fractional part of baud rate
    //    FIXME: This works because PL011 is only used
    //    on the virt machine, fix for real hardware
    iowrite32(&r->IBRD, 0);
    iowrite32(&r->FBRD, 0);

    static constexpr uint32_t WORD_LENGTH_EIGHT_BITS = 0b11 << 5;
    static constexpr uint32_t FIFO_ENABLED = 1 << 4;
    iowrite32(&r->LCR_H, FIFO_ENABLED | WORD_LENGTH_EIGHT_BITS);

    static constexpr uint32_t UART_ENABLE = 1 << 0;
    static constexpr uint32_t TRANSMIT_ENABLE = 1 << 8;
    iowrite32(&r->CR, UART_ENABLE | TRANSMIT_ENABLE);

    return 0;
}

int32_t PL011UART::init_for_early_boot()
{
    return init_except_interrupt();
}

int32_t PL011UART::init()
{
    if (m_fully_initialized)
        return 0;

    int32_t err = init_except_interrupt();
    if (err)
        return err;

    uint32_t cr = ioread32(&r->CR);
    cr |= 1 << 9;                       // Enable uart receiver
    iowrite32(&r->CR, cr);

    irq_install(m_config.irq, [](auto*, void *arg) { static_cast<PL011UART*>(arg)->irq_handler(); }, this);

    iowrite32(&r->IMSC, RXINTR_MASK);
    iowrite32(&r->ICR, 0x7ff);          // Clear all IRQs, if they were set
    irq_mask(m_config.irq, false);

    m_fully_initialized = true;

    return 0;
}

int32_t PL011UART::shutdown()
{
    todo();
    return 0;
}

int64_t PL011UART::write(const uint8_t *buffer, size_t size)
{
    if (r == nullptr)
        return -EIO;

    static constexpr uint32_t TRANSMIT_FIFO_FULL = 1 << 5;
    for (size_t i = 0; i < size; i++) {
        while (ioread32(&r->FR) & TRANSMIT_FIFO_FULL)
            cpu_relax();
        iowrite32(&r->DR, buffer[i]);
    }

    return size;
}


int64_t PL011UART::read(uint8_t*, size_t)
{
    if (!m_fully_initialized) {
        return -EIO;
    }

    return 0;
}

int32_t PL011UART::ioctl(uint32_t, void*)
{
    return -ENOTSUP;
}

void PL011UART::irq_handler()
{
    if (ioread32(&r->RIS) & RXINTR_MASK) {
        while (!(ioread32(&r->FR) & RXFE_MASK)) {
            uint8_t data = ioread32(&r->DR) & 0xff;
            kprintf("'%c'\n", data);
        }
        iowrite32(&r->ICR, RXINTR_MASK);
    }
}
