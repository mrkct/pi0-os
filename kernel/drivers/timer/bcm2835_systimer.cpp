#include <kernel/drivers/irqc/bcm2835_irqc.h>
#include "bcm2835_systimer.h"



/*
    NOTE: The BCM2835 ARM Peripherals manual is incomplete.
    Consider these facts:
    - Channels 0 and 2 are reserved for GPU usage and we should not
      touch them from the ARM side
    - Channels 1 and 3 are available for ARM usage and their IRQ numbers
      are 1 and 3 of the IRQ Basic Pending 1 register

    These facts come from this site:
    https://xinu.cs.mu.edu/index.php/BCM2835_Interrupt_Controller
*/
static constexpr uint32_t CHANNEL       = 1;
static constexpr uint32_t CHANNEL_IRQ   = BCM2835InterruptController::irq(
    BCM2835InterruptController::Group::Pending1, 1
);


BCM2835SystemTimer::BCM2835SystemTimer(Config const *config)
    : m_config(*config)
{
}

int32_t BCM2835SystemTimer::init()
{
    if (r != nullptr)
        return -ERR_ALREADY;
    
    r = reinterpret_cast<RegisterMap volatile*>(ioremap(m_config.address, sizeof(RegisterMap)));

    irq_install(CHANNEL_IRQ, [](auto *frame, void *arg) {
        static_cast<BCM2835SystemTimer*>(arg)->irq_handler(frame, CHANNEL_IRQ);
    }, this);
    irq_mask(CHANNEL_IRQ, false);

    return 0;
}

void BCM2835SystemTimer::irq_handler(InterruptFrame *frame, uint32_t channel)
{
    if (m_handler != nullptr)
        m_handler(frame, *this, m_ticks_at_last_irq, m_handler_arg);
    m_ticks_at_last_irq = ticks();
    
    iowrite32(&r->cs, 0xff);

    uint32_t chan = ioread32(&r->c[CHANNEL]);
    iowrite32(&r->c[CHANNEL], chan + m_period);
}

int32_t BCM2835SystemTimer::shutdown()
{
    todo();
    return 0;
}

uint64_t BCM2835SystemTimer::ticks() const
{
    uint64_t high, low;
    kassert(r != nullptr);
    do {
        high = ioread32(&r->chi);
        low = ioread32(&r->clo);
        if (high == ioread32(&r->chi))
            return (high << 32) | low;
    } while (true);
    kassert_not_reached();
}

uint64_t BCM2835SystemTimer::ticks_per_ms() const
{
    return m_config.clock_frequency / 1000;
}

void BCM2835SystemTimer::start(uint64_t ticks, SystemTimerCallback handler, void *arg)
{
    kassert(r != nullptr);
    kassert(m_handler == nullptr);
    m_handler = handler;
    m_handler_arg = arg;
    m_period = ticks;

    iowrite32(&r->cs, 1 << CHANNEL);
    uint32_t current_ticks = ioread32(&r->clo);
    iowrite32(&r->c[CHANNEL], current_ticks + ticks);
}
