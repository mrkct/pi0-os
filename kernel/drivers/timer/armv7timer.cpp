#include <kernel/arch/arm/arch.h>

#include "armv7timer.h"

#define LOG_ENABLED
#define LOG_TAG "ARM7TIM"
#include <kernel/log.h>


ARMv7Timer::ARMv7Timer(Config const *config)
    : m_config(*config)
{
}

int32_t ARMv7Timer::init()
{
    ARM_MRC(p15, 0, m_clock_frequency, c14, c0, 0);
    LOGI("Clock frequency: %u Hz", m_clock_frequency);

    uint32_t cntp_ctl = 1; // Enable timer
    ARM_MCR(p15, 0, cntp_ctl, c14, c2, 1);

    irq_install(m_config.irq, [](auto *frame, void *arg) {
        static_cast<ARMv7Timer*>(arg)->irq_handler(frame);
    }, this);

    return 0;
}

uint64_t ARMv7Timer::ticks() const
{
    uint32_t low, high;
    ARM_MRRC(p15, 0, low, high, c14);
    return (static_cast<uint64_t>(high) << 32) | low;
}

void ARMv7Timer::reload_countdown_timer()
{
    uint32_t ticks = static_cast<uint32_t>(m_period);
    ARM_MCR(p15, 0, ticks, c14, c2, 0);
}

void ARMv7Timer::start(uint64_t period, SystemTimerCallback handler, void *arg)
{
    LOGI("Starting timer with period %u ms", static_cast<uint32_t>(period));
    m_handler = handler;
    m_handler_arg = arg;
    m_period = period;
    reload_countdown_timer();
    irq_mask(m_config.irq, false);
}

void ARMv7Timer::irq_handler(InterruptFrame *frame)
{
    if (m_handler != nullptr)
        m_handler(frame, *this, m_ticks_at_last_irq, m_handler_arg);
    m_ticks_at_last_irq = ticks();
    reload_countdown_timer();
}
