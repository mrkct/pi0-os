#pragma once

#include <kernel/error.h>
#include <kernel/interrupt.h>
#include <stdint.h>

namespace kernel {

enum class SystimerChannel {
    Channel1,
    Channel3,
};

typedef void (*SystimerCallback)(InterruptFrame*);

void systimer_init();

uint64_t systimer_get_ticks();

Error systimer_install_handler(SystimerChannel, SystimerCallback);

Error systimer_trigger(SystimerChannel, uint32_t ticks);

int64_t systimer_ms_to_ticks(uint32_t ms);

}
