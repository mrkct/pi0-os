#include "bcm2835_irqc.h"


BCM2835InterruptController::BCM2835InterruptController(Config const* config)
    : m_iobase(config->iobase), m_offset(config->offset)
{
}

int32_t BCM2835InterruptController::init()
{
    r = reinterpret_cast<RegisterMap volatile*>(ioremap(m_iobase + m_offset, sizeof(RegisterMap)));

    iowrite32(r->fiq_control, 0);
    iowrite32(r->disable_irq1, 0xffffffff);
    iowrite32(r->disable_irq2, 0xffffffff);
    iowrite32(r->disable_basic_irq, 0xffffffff);

    return 0;
}

void BCM2835InterruptController::mask_interrupt(uint32_t irqidx)
{
    auto irqd = IrqDescriptor::from_idx(irqidx);
    switch (irqd.group) {
    case IrqDescriptor::Group::Basic:
        iowrite32(r->disable_basic_irq, 1 << irqd.irq);
        break;
    case IrqDescriptor::Group::Pending1:
        iowrite32(r->disable_irq1, 1 << irqd.irq);
        break;
    case IrqDescriptor::Group::Pending2:
        iowrite32(r->disable_irq2, 1 << irqd.irq);
        break;
    }
}

void BCM2835InterruptController::unmask_interrupt(uint32_t irqidx)
{
    auto irqd = IrqDescriptor::from_idx(irqidx);
    switch (irqd.group) {
    case IrqDescriptor::Group::Basic:
        iowrite32(r->enable_basic_irq, 1 << irqd.irq);
        break;
    case IrqDescriptor::Group::Pending1:
        iowrite32(r->enable_irq1, 1 << irqd.irq);
        break;
    case IrqDescriptor::Group::Pending2:
        iowrite32(r->enable_irq2, 1 << irqd.irq);
        break;
    }
}

void BCM2835InterruptController::install_irq(uint32_t irqidx, InterruptHandler handler, void *arg)
{
    auto irqd = IrqDescriptor::from_idx(irqidx);
    switch (irqd.group) {
    case IrqDescriptor::Group::Basic:
        kassert(irqd.irq < array_size(m_basic_irqs));
        kassert(m_basic_irqs[irqd.irq].handler == nullptr);
        m_basic_irqs[irqd.irq] = { handler, arg };
        break;
    case IrqDescriptor::Group::Pending1:
        kassert(irqd.irq < array_size(m_pending1_irqs));
        kassert(m_pending1_irqs[irqd.irq].handler == nullptr);
        m_pending1_irqs[irqd.irq] = { handler, arg };
        break;
    case IrqDescriptor::Group::Pending2:
        kassert(irqd.irq < array_size(m_pending2_irqs));
        kassert(m_pending2_irqs[irqd.irq].handler == nullptr);
        m_pending2_irqs[irqd.irq] = { handler, arg };
        break;
    }
}

void BCM2835InterruptController::dispatch_irq(InterruptFrame *frame)
{
    kassert_no_print(!irq_enabled());
    uint32_t basic, pending1, pending2;
    basic = ioread32(r->basic_pending);
    pending1 = ioread32(r->pending1);
    pending2 = ioread32(r->pending2);

    static constexpr uint32_t ONE_OR_MORE_BITS_SET_IN_PENDING1 = 1 << 8;
    static constexpr uint32_t ONE_OR_MORE_BITS_SET_IN_PENDING2 = 1 << 9;

    bool also_check_pending1 = basic & ONE_OR_MORE_BITS_SET_IN_PENDING1;
    bool also_check_pending2 = basic & ONE_OR_MORE_BITS_SET_IN_PENDING2;

    auto const& call_irq = [](Irq &irq) {
        kassert(irq.handler != nullptr);
        irq.handler(irq.arg);
    };

    // We only care about the first 10 bits because the rest are repeated
    // IRQs also readable from the "pending" registers
    for (int i = 0; i < 10; ++i) {
        auto mask = 1 << i;
        if (mask == ONE_OR_MORE_BITS_SET_IN_PENDING1 || mask == ONE_OR_MORE_BITS_SET_IN_PENDING2)
            continue;

        if (basic & mask)
            call_irq(m_basic_irqs[i]);
    }

    if (also_check_pending1) {
        for (int i = 0; i < 32; ++i) {
            if (pending1 & (1 << i))
                call_irq(m_pending1_irqs[i]);
        }
    }

    if (also_check_pending2) {
        for (int i = 0; i < 32; ++i) {
            if (pending2 & (1 << i))
                call_irq(m_pending2_irqs[i]);
        }
    }
}
