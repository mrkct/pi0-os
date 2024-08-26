#pragma once

#include <kernel/drivers/device.h>


class GlobalInterruptController2: public InterruptController
{
public:
    struct Config {
        uintptr_t distributor_address;
        uintptr_t cpu_interface_address;
    };

    GlobalInterruptController2(Config const* config);

    virtual const char *name() const override { return "GICv2 Interrupt Controller"; }

    virtual int32_t init() override;

    virtual void mask_interrupt(uint32_t irqidx) override;
    virtual void unmask_interrupt(uint32_t irqidx) override;
    virtual void install_irq(uint32_t irqidx, InterruptHandler handler, void *arg) override;
    virtual void dispatch_irq(InterruptFrame *frame) override;

private:
    struct DistributorRegisterMap {
        uint32_t ctlr;
        uint32_t typer;
        uint32_t iidr;
        uint8_t  reserved1[0x80 - 0x0c];
        uint32_t igroup[32];
        uint32_t isenable[32];
        uint32_t icenable[32];
        uint32_t ispend[32];
        uint32_t icpend[32];
        uint32_t isactive[32];
        uint32_t icactive[32];
        uint32_t ipriority[255];
        uint32_t reserved2;
        uint32_t itargets[8];
        uint8_t  reserved3[0xc00 - 0x820];
        uint32_t icfg[64];
        uint8_t  reserved4[0xe00 - 0xd00];
        uint32_t nsac[64];
        uint32_t sgir;
        uint8_t  reserved5[0xf10 - 0xf04];
        uint32_t cpendsgi[4];
        uint32_t spendsgi[4];
        uint8_t  reserved6[0xfd0 - 0xf30];
        uint32_t identification[12];
    } __attribute__((packed));
    static_assert(offsetof(DistributorRegisterMap, iidr) == 0x08);
    static_assert(offsetof(DistributorRegisterMap, igroup) == 0x80);
    static_assert(offsetof(DistributorRegisterMap, isenable) == 0x100);
    static_assert(offsetof(DistributorRegisterMap, itargets) == 0x800);
    static_assert(offsetof(DistributorRegisterMap, icfg) == 0xc00);
    static_assert(offsetof(DistributorRegisterMap, nsac) == 0xe00);
    static_assert(offsetof(DistributorRegisterMap, identification) == 0xfd0);
    static_assert(sizeof(DistributorRegisterMap) == 0x1000);

    struct CPURegisterMap {
        uint32_t ctlr;
        uint32_t pmr;
        uint32_t bpr;
        uint32_t iar;
        uint32_t eoir;
        uint32_t rpr;
        uint32_t hppir;
        uint32_t abpr;
        uint32_t aiar;
        uint32_t aeoir;
        uint32_t ahppir;
        uint8_t  reserved[0xd0 - 0x2c];
        uint32_t apr[4];
        uint32_t nsapr[4];
        uint8_t  reserved2[12];
        uint32_t iidr;
        uint8_t  reserved3[0x1000 - 0xfc - 4];
        uint32_t dir;
    } __attribute__((packed));
    static_assert(offsetof(CPURegisterMap, eoir) == 0x10);
    static_assert(offsetof(CPURegisterMap, ahppir) == 0x28);
    static_assert(offsetof(CPURegisterMap, apr) == 0xd0);
    static_assert(offsetof(CPURegisterMap, nsapr) == 0xe0);
    static_assert(offsetof(CPURegisterMap, iidr) == 0xfc);
    static_assert(offsetof(CPURegisterMap, dir) == 0x1000);
    static_assert(sizeof(CPURegisterMap) == 0x1004);

    struct IrqHandler {
        InterruptHandler handler;
        void *arg;
    };
    IrqHandler m_handlers[256];

    Config m_config;
    DistributorRegisterMap volatile *rd;
    CPURegisterMap volatile *rc;
};
