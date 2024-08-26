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

        static IrqDescriptor from_idx(uint32_t idx) {
            return IrqDescriptor {
                .group = static_cast<Group>(idx / 32),
                .irq = idx % 32,
            };
        }

        uint32_t idx() const {
            return (group == Group::Basic ? 0 : group == Group::Pending1 ? 1 : 2) * 32 + irq;
        }
    };

    virtual const char *name() const override { return "BCM2835 Interrupt Controller"; }

    virtual int32_t init() override;

    virtual void mask_interrupt(uint32_t irqidx) override;
    virtual void unmask_interrupt(uint32_t irqidx) override;
    virtual void install_irq(uint32_t irqidx, InterruptHandler handler, void *arg) override;
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
