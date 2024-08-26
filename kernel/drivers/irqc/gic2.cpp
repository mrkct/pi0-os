#include "gic2.h"


static constexpr uint32_t RESERVED_IRQS = 32;

GlobalInterruptController2::GlobalInterruptController2(Config const *config)
    :   InterruptController(), m_config(*config)
{
}

int32_t GlobalInterruptController2::init()
{
    rd = reinterpret_cast<DistributorRegisterMap volatile*>(ioremap(m_config.distributor_address, sizeof(DistributorRegisterMap)));
    rc = reinterpret_cast<CPURegisterMap volatile*>(ioremap(m_config.cpu_interface_address, sizeof(CPURegisterMap)));

    iowrite32(&rd->ctlr, 0b11);  // Forward group 1 & 2 interrupts to cpu interface
    iowrite32(&rc->ctlr, 0b11);  // Enable group 1 interrupts
    iowrite32(&rc->pmr, 0xff);   // Set priority mask to highest level (don't filter anything)

    return 0;
}

void GlobalInterruptController2::mask_interrupt(uint32_t irqidx)
{
    iowrite32(&rd->icenable[(RESERVED_IRQS + irqidx) / 32], 1 << (irqidx % 32));
}

void GlobalInterruptController2::unmask_interrupt(uint32_t irqidx)
{
    iowrite32(&rd->isenable[(RESERVED_IRQS + irqidx) / 32], 1 << (irqidx % 32));
}

void GlobalInterruptController2::install_irq(uint32_t irqidx, InterruptHandler handler, void *arg)
{
    m_handlers[irqidx].handler = handler;
    m_handlers[irqidx].arg = arg;
}

void GlobalInterruptController2::dispatch_irq(InterruptFrame *frame)
{
    while (true) {
        uint32_t iar = ioread32(&rc->iar);
        uint32_t irqidx = iar & 0x3ff;
        if (irqidx >= 1020)
            break;

        kassert(irqidx >= RESERVED_IRQS);
        irqidx -= RESERVED_IRQS;

        if (m_handlers[irqidx].handler == nullptr)
            panic("Unhandled interrupt %u", irqidx);

        m_handlers[irqidx].handler(m_handlers[irqidx].arg);

        iowrite32(&rc->eoir, iar);
    }
}
