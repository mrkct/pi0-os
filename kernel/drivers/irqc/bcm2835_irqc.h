#pragma once

#include <kernel/drivers/device.h>


class BCM2835InterruptController: public InterruptController
{
public:
    struct Config {
        uintptr_t iobase;
        uintptr_t offset;
    };

    BCM2835InterruptController(Config const* config);

    struct IrqDescriptor {
        enum class Group { Basic, Pending1, Pending2 };
        Group group;
        uint32_t irq;
    };

    virtual int32_t init() override;
    virtual int32_t shutdown() override { panic("You can't shutdown the interrupt controller!"); }

    virtual void mask_interrupt(void *irq) override;
    virtual void unmask_interrupt(void *irq) override;
    virtual void install_irq(void *irq_descriptor, InterruptHandler handler, void *arg) override;
    virtual void dispatch_irq(InterruptFrame *frame) override;
private:
    struct RegisterMap {
        uint32_t basic_pending;
        uint32_t pending1;
        uint32_t pending2;
        uint32_t fiq_control;
        uint32_t enable_irq1;
        uint32_t enable_irq2;
        uint32_t enable_basic_irq;
        uint32_t disable_irq1;
        uint32_t disable_irq2;
        uint32_t disable_basic_irq;
    };
    static_assert(sizeof(RegisterMap) == 0x28);

    struct Irq {
        InterruptHandler handler;
        void *arg;
    };

    uintptr_t m_iobase;
    uintptr_t m_offset;
    RegisterMap volatile *r;
    
    Irq m_basic_irqs[32];
    Irq m_pending1_irqs[32];
    Irq m_pending2_irqs[32];
};
