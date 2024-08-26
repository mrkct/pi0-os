#pragma once

#include <kernel/drivers/device.h>


class BCM2835GPIOController: public GPIOController
{
private:
    static constexpr uint32_t PIN_COUNT = 50;

public:
    struct Config {
        uintptr_t iobase;
        uintptr_t offset;
    };

    BCM2835GPIOController(const Config *config);

    virtual int32_t init() override;
    virtual int32_t init_for_early_boot() override;
    virtual int32_t shutdown() override;

    virtual int32_t configure_pin(uint32_t port, uint32_t pin, PinFunction function) override;
    virtual int32_t configure_pin_pull_up_down(uint32_t port, uint32_t pin, PullState) override;

    virtual int32_t get_port_count() override { return 1; }
    virtual int32_t get_port_pin_count(uint32_t) override { return PIN_COUNT; }
    virtual int32_t get_pin_state(uint32_t port, uint32_t pin) override;
    virtual int32_t set_pin_state(uint32_t port, uint32_t pin, PinState state) override;

private:
    struct RegisterMap {
        uint32_t GPFSEL[6];
        uint32_t reserved;
        uint32_t GPSET[2];
        uint32_t reserved2;
        uint32_t GPCLR[2];
        uint32_t reserved3;
        uint32_t GPLEV[2];
        uint32_t reserved4;
        uint32_t GPEDS[2];
        uint32_t reserved5;
        uint32_t GPREN[2];
        uint32_t reserved6;
        uint32_t GPFEN[2];
        uint32_t reserved7;
        uint32_t GPHEN[2];
        uint32_t reserved8;
        uint32_t GPLEN[2];
        uint32_t reserved9;
        uint32_t GPAREN[2];
        uint32_t reserved10;
        uint32_t GPAFEN[2];
        uint32_t reserved11;
        uint32_t GPPUD;
        uint32_t GPPUDCLK[2];
        uint32_t reserved12;
        uint32_t test;
    };
    static_assert(offsetof(RegisterMap, GPCLR) == 0x28);
    static_assert(offsetof(RegisterMap, GPREN) == 0x4c);
    static_assert(offsetof(RegisterMap, GPPUD) == 0x94);

    uintptr_t m_iobase;
    uintptr_t m_offset;
    RegisterMap volatile *r;
};
