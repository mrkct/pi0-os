#pragma once

#include <kernel/arch/arch.h>


typedef void (*InterruptHandler)(InterruptFrame*, void*);

void irq_init();

/**
 * @brief Dispatches an IRQ using the system's IRQ controller
 * 
 * This function is called by the architecture-specific IRQ code
*/
void dispatch_irq(InterruptFrame*);

static inline bool irq_enabled() { return arch_irq_enabled(); }

static inline void irq_enable() { arch_irq_enable(); }

static inline void irq_disable() { arch_irq_disable(); }

void irq_install(uint32_t irq, InterruptHandler, void *arg);

void irq_mask(uint32_t irq, bool mask);
