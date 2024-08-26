#include <kernel/drivers/device.h>
#include <kernel/drivers/devicemanager.h>

#include "irq.h"


void irq_init()
{
    arch_irq_init();
    irq_enable();
}

void irq_install(uint32_t irq, InterruptHandler handler, void *arg)
{
    auto *irqc = devicemanager_get_interrupt_controller_device();
    kassert(irqc != nullptr);
    irqc->install_irq(irq, handler, arg);
}

void dispatch_irq(InterruptFrame *frame)
{
    auto *irqc = devicemanager_get_interrupt_controller_device();
    kassert(irqc != nullptr);
    irqc->dispatch_irq(frame);
}

void irq_mask(uint32_t irq, bool mask)
{
    auto *irqc = devicemanager_get_interrupt_controller_device();
    kassert(irqc != nullptr);
    if (mask)
        irqc->mask_interrupt(irq);
    else
        irqc->unmask_interrupt(irq);
}
