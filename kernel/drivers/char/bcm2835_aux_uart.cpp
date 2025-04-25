#include "bcm2835_aux_uart.h"


BCM2835AuxUART::BCM2835AuxUART(Config const *config)
    : UART(), m_config(*config)
{
}

int32_t BCM2835AuxUART::init_except_interrupt()
{
    if (r != nullptr)
        return 0;

    r = static_cast<BCM2835AuxRegisterMap volatile*>(ioremap(m_config.iobase + m_config.offset, sizeof(BCM2835AuxRegisterMap)));

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

    return 0;
}

int32_t BCM2835AuxUART::init_for_early_boot()
{
    return init_except_interrupt();
}

int32_t BCM2835AuxUART::init()
{
    if (m_initialized)
        return 0;

    int32_t rc = init_except_interrupt();
    if (rc != 0)
        return rc;

    // Check errata: TX and RX bits are swapped, and you also need to set bits 3:2
    static constexpr uint32_t RX_IRQ_ENABLE = 1 | (1 << 2);
    auto ier = ioread32(&r->mu_ier_reg);
    ier |= RX_IRQ_ENABLE;
    iowrite32(&r->mu_ier_reg, ier);

    irq_install(m_config.irq, [](auto*, void *arg) { reinterpret_cast<BCM2835AuxUART*>(arg)->irq_handler(); }, this);
    irq_mask(m_config.irq, false);

    m_initialized = true;

    return 0;
}

int32_t BCM2835AuxUART::shutdown()
{
    todo();
    return 0;
}

void BCM2835AuxUART::echo_raw(uint8_t c)
{
    static constexpr uint32_t TRANSMITTER_EMPTY = 1 << 5;
    while (!(ioread32(&r->mu_lsr_reg) & TRANSMITTER_EMPTY))
	    cpu_relax();

    iowrite32(&r->mu_io_reg, c);
}

void BCM2835AuxUART::irq_handler()
{
    static constexpr uint32_t IRQ_PENDING = 1;
    if ((ioread32(&r->mu_iir_reg) & IRQ_PENDING) != 0)
        return;
        
    auto irq_source = ioread32(&r->mu_iir_reg);
    
    // Check if it is a UART "Receiver holds valid byte" interrupt
    if ((irq_source >> 1 & 0b11) == 0b10) {
        uint8_t data = ioread32(&r->mu_io_reg) & 0xff;
        emit(data);
    }
}
