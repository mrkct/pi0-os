#pragma once

#include <kernel/drivers/device.h>


class BCM2835SystemTimer : public SystemTimer
{
public:
    struct Config {
        uintptr_t address;
        uint64_t clock_frequency;
    };
    BCM2835SystemTimer(Config const *config);

    virtual const char *name() const { return "BCM2835 System Timer"; }
    virtual int32_t init() override;
    virtual int32_t shutdown() override;

    virtual uint64_t ticks() const override;
    virtual uint64_t ticks_per_ms() const override;
    virtual void start(uint64_t ticks, SystemTimerCallback, void *arg) override;

private:
    struct RegisterMap {
        uint32_t cs;
        uint32_t clo;
        uint32_t chi;
        uint32_t c[4];
    };

    uint64_t read_ticks_counter() const;

    void irq_handler(InterruptFrame *frame, uint32_t channel);

    Config m_config;
    SystemTimerCallback m_handler = nullptr;
    void *m_handler_arg = nullptr;

    uint64_t m_period = 0;
    uint64_t m_ticks_at_last_irq = 0;
    RegisterMap volatile *r = nullptr;
};
