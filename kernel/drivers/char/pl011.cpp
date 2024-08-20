#include "pl011.h"


PL011UART::PL011UART(uintptr_t peripheral_phys_addr)
    :   UART(), m_peripheral_physical_address(peripheral_phys_addr)
{
}

int32_t PL011UART::init()
{
    if (m_initialized) {
        return 0;
    }

    r = static_cast<RegisterMap volatile*>(ioremap(m_peripheral_physical_address, sizeof(RegisterMap)));

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

    m_initialized = true;

    return 0;
}

int32_t PL011UART::shutdown()
{
    todo();
    return 0;
}

int64_t PL011UART::write(const uint8_t *buffer, size_t size)
{
    if (!m_initialized) {
        return -EIO;
    }

    static constexpr uint32_t TRANSMIT_FIFO_FULL = 1 << 5;
    for (size_t i = 0; i < size; i++) {
        while (ioread32(&r->FR) & TRANSMIT_FIFO_FULL)
            ;
        iowrite32(&r->DR, buffer[i]);
    }

    return size;
}


int64_t PL011UART::read(uint8_t*, size_t)
{
    if (!m_initialized) {
        return -EIO;
    }

    return 0;
}

int32_t PL011UART::ioctl(uint32_t, void*)
{
    return -ENOTSUP;
}
