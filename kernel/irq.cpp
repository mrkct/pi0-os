#include <kernel/drivers/device.h>
#include <kernel/drivers/devicemanager.h>

#include "irq.h"


static InterruptController *s_irqc;

void irq_init()
{
    arch_irq_init();
    s_irqc = devicemanager_get_interrupt_controller_device();
    irq_enable();
}

void irq_install(void *irq, InterruptHandler handler, void *arg)
{
    kassert(s_irqc != nullptr);
    s_irqc->install_irq(irq, handler, arg);
}

void dispatch_irq(InterruptFrame *frame)
{
    kassert(s_irqc != nullptr);
    s_irqc->dispatch_irq(frame);
}
