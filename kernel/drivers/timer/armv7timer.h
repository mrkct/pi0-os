#pragma once

#include <kernel/drivers/device.h>


class ARMv7Timer : public SystemTimer
{
public:
    struct Config {
        uint32_t irq;
    };
    ARMv7Timer(Config const *config);

    virtual const char *name() const { return "ARMv7 Timer"; }
    virtual int32_t init() override;

    virtual uint64_t ticks() const override;
    virtual uint64_t ticks_per_ms() const override { return m_clock_frequency / 1000; }
    virtual void start(uint64_t ticks, SystemTimerCallback, void *arg) override;

private:
    void irq_handler(InterruptFrame *frame);

    void reload_countdown_timer();

    Config m_config;
    SystemTimerCallback m_handler = nullptr;
    void *m_handler_arg = nullptr;

    uint32_t m_clock_frequency = 0;
    uint64_t m_period = 0;
    uint64_t m_ticks_at_last_irq = 0;
};
