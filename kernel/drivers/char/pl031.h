#pragma once

#include <kernel/drivers/device.h>


class PL031: public RealTimeClock
{
private:
    struct RegisterMap {
        uint32_t dr;
        uint32_t mr;
        uint32_t lr;
        uint32_t cr;
        uint32_t imsc;
        uint32_t ris;
        uint32_t mis;
        uint32_t icr;
        uint8_t reserved[0xfe0 - 0x20];
        uint32_t periphID[4];
        uint32_t cellID[4];
    };
    static_assert(sizeof(RegisterMap) == 0x1000);
    static_assert(offsetof(RegisterMap, icr) == 0x1c);
    static_assert(offsetof(RegisterMap, periphID) == 0xfe0);

public:
    struct Config {
        uintptr_t physaddr;
    };

    PL031(Config const *config);

    virtual int32_t init() override;
    virtual int32_t shutdown() override;

    virtual int32_t get_time(DateTime&) override;
    virtual int32_t set_time(const DateTime) override;

private:
    Config m_config;
    RegisterMap volatile *r;
};
