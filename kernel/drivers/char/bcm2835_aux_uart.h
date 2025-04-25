#pragma once

#include <kernel/drivers/device.h>
#include "bcm2835_aux.h"


class BCM2835AuxUART: public UART
{

public:
    struct Config {
        uintptr_t iobase;
        uintptr_t offset;
        uint32_t irq;
    };

    BCM2835AuxUART(Config const *config);

    virtual int32_t init() override;
    virtual int32_t init_for_early_boot() override;
    virtual int32_t shutdown() override;

protected:
    virtual void echo_raw(uint8_t ch) override;  

private:
    int32_t init_except_interrupt();

    void irq_handler();

    bool m_initialized { false };
    Config m_config;
    BCM2835AuxRegisterMap volatile *r;
};
