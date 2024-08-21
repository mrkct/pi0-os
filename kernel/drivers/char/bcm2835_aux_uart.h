#pragma once

#include <kernel/drivers/device.h>
#include "bcm2835_aux.h"


class BCM2835AuxUART: public UART
{

public:
    struct Config {
        uintptr_t iobase;
        uintptr_t offset;
    };

    BCM2835AuxUART(Config const *config);

    virtual int32_t init() override;
    virtual int32_t shutdown() override;

    virtual int32_t ioctl(uint32_t request, void *argp) override;

protected:
    virtual int64_t read(uint8_t *buffer, size_t size) override;
    virtual int64_t write(const uint8_t *buffer, size_t size) override;    

private:
    int64_t writebyte(uint8_t c);

    bool m_initialized { false };
    
    uintptr_t m_iobase;
    uintptr_t m_offset;
    
    BCM2835AuxRegisterMap volatile *r;
};
