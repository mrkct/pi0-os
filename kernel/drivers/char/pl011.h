#pragma once

#include <kernel/drivers/device.h>


class PL011UART: public UART
{
private:
    struct RegisterMap {
        uint32_t DR;
        union {
            uint32_t RSR;
            uint32_t ECR;
        };
        uint32_t reserved0[4];
        uint32_t FR;
        uint32_t reserved1[1];
        uint32_t ILPR;
        uint32_t IBRD;
        uint32_t FBRD;
        uint32_t LCR_H;
        uint32_t CR;
        uint32_t IFLS;
        uint32_t IMSC;
        uint32_t RIS;
        uint32_t MIS;
        uint32_t ICR;
        uint32_t DMACR;
        uint8_t reserved[3988];
        uint32_t PeriphID[4];
        uint32_t CellID[4];
    };
    static_assert(sizeof(RegisterMap) == 0x1000);
    static_assert(offsetof(RegisterMap, RIS) == 0x3c);
    static_assert(offsetof(RegisterMap, PeriphID) == 0xfe0);

public:
    struct Config {
        uintptr_t physaddr;
        uint32_t irq;
    };

    PL011UART(Config const *config);

    virtual int32_t init() override;
    virtual int32_t init_for_early_boot() override;
    virtual int32_t shutdown() override;

protected:
    virtual void echo_raw(uint8_t ch) override;  

private:
    int32_t init_except_interrupt();

    void irq_handler();

    bool m_fully_initialized { false };
    Config m_config;
    RegisterMap volatile *r;
};
