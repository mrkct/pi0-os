#include "irqlock.h"


IrqLock irq_lock()
{
    IrqLock were_enabled = irq_enabled();
    irq_disable();
    return were_enabled;
}

void release(IrqLock were_enabled)
{
    if (were_enabled)
        irq_enable();
}
