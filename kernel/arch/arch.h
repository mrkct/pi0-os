#pragma once

#ifndef UNIT_TEST

#include "arm/arch.h"

#else

struct InterruptFrame {
    int useless;
};

static inline bool arch_irq_enabled() { return true; }

static inline void arch_irq_enable() { }

static inline void arch_irq_disable() { }

#endif
