#include "bcm2835_aux_uart.h"


BCM2835AuxUART::BCM2835AuxUART(Config const *config)
    : UART(), m_iobase(config->iobase), m_offset(config->offset)
{
}

int32_t BCM2835AuxUART::init()
{
    if (m_initialized) {
        return 0;
    }

    r = static_cast<BCM2835AuxRegisterMap volatile*>(ioremap(m_iobase + m_offset, sizeof(BCM2835AuxRegisterMap)));

    static constexpr uint32_t MINI_UART_ENABLED = 1;
    iowrite32(&r->enables, MINI_UART_ENABLED);

    iowrite32(&r->mu_ier_reg, 0);

    // Disable TX and RX while setting up the UART
    iowrite32(&r->mu_cntl_reg, 0);
    
    // WARNING: Check the errata for this register
    static constexpr uint32_t EIGHT_BIT_DATA_SIZE = 0b11;
    iowrite32(&r->mu_lcr_reg, EIGHT_BIT_DATA_SIZE);

    static constexpr uint32_t RTS_HIGH = 0;
    iowrite32(&r->mu_mcr_reg, RTS_HIGH);

    iowrite32(&r->mu_ier_reg, 0);

    static constexpr uint32_t CLEAR_FIFOS = 0xc6;
    iowrite32(&r->mu_iir_reg, CLEAR_FIFOS);
    
    static constexpr uint32_t divisor = 250000000 / (8 * 115200) - 1;
    iowrite32(&r->mu_baud_reg, divisor);

    static constexpr uint32_t TX_ENABLE = 1 << 1;
    static constexpr uint32_t RX_ENABLE = 1 << 0;
    iowrite32(&r->mu_cntl_reg, TX_ENABLE | RX_ENABLE);

    m_initialized = true;

    return 0;
}

int32_t BCM2835AuxUART::shutdown()
{
    todo();
    return 0;
}

int64_t BCM2835AuxUART::writebyte(uint8_t c)
{
    static constexpr uint32_t TRANSMITTER_EMPTY = 1 << 5;
    while (!(ioread32(&r->mu_lsr_reg) & TRANSMITTER_EMPTY))
	    ;
	
    iowrite32(&r->mu_io_reg, c);
    return 0;
}

int64_t BCM2835AuxUART::write(const uint8_t *buffer, size_t size)
{
    if (!m_initialized) {
        return -EIO;
    }

    int64_t rc = 0;

    for (size_t i = 0; i < size; i++) {
        rc = writebyte(buffer[i]);
        if (rc != 0) {
            return rc;
        }
    } 

    return size;
}


int64_t BCM2835AuxUART::read(uint8_t*, size_t)
{
    if (!m_initialized) {
        return -EIO;
    }

    todo();

    return 0;
}

int32_t BCM2835AuxUART::ioctl(uint32_t, void*)
{
    return -ENOTSUP;
}
