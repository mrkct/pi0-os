#include <kernel/device/systimer.h>
#include <kernel/memory/kheap.h>

namespace kernel {

constexpr uint32_t MS_BETWEEN_INTERRUPTS = 75;

struct TimerEvent {
    int64_t time_to_exec;
    void (*callback)(void*);
    void* data;

    TimerEvent* next;
};

static uint64_t g_time_passed_since_boot_in_ms = 0;
static TimerEvent* g_active_timer_events = nullptr;

static void systimer_callback()
{
    g_time_passed_since_boot_in_ms += MS_BETWEEN_INTERRUPTS;

    TimerEvent* prev = nullptr;
    TimerEvent* event = g_active_timer_events;

    // kprintf("------\n");
    while (event) {
        event->time_to_exec -= MS_BETWEEN_INTERRUPTS;
        // kprintf("Time to exec: %d\n", (int) event->time_to_exec);

        if (event->time_to_exec <= 0) {
            event->callback(event->data);

            if (prev) {
                prev->next = event->next;
            } else {
                g_active_timer_events = event->next;
            }

            kfree(event);
            event = prev ? prev->next : g_active_timer_events;
        } else {
            prev = event;
            event = event->next;
        }
    }
}

void timer_init()
{
    MUST(systimer_install_handler(SystimerChannel::Channel1, [](auto*) {
        auto ticks_before = systimer_get_ticks();
        systimer_callback();
        auto ticks_after = systimer_get_ticks();
        // kprintf("before: %d    after: %d\n", (int) ticks_before, (int) ticks_after);

        uint32_t ticks_passed;
        if (ticks_after < ticks_before) {
            ticks_passed = ticks_after + (0xffffffff - ticks_before);
        } else {
            ticks_passed = ticks_after - ticks_before;
        }

        systimer_trigger(SystimerChannel::Channel1, systimer_ms_to_ticks(MS_BETWEEN_INTERRUPTS));
    }));
    systimer_trigger(SystimerChannel::Channel1, systimer_ms_to_ticks(MS_BETWEEN_INTERRUPTS));
}

uint64_t timer_time_passed_since_boot_in_ms()
{
    return g_time_passed_since_boot_in_ms;
}

Error timer_exec_after(uint32_t ms, void (*callback)(void*), void* data)
{
    TimerEvent* event;

    TRY(kmalloc(sizeof(*event), event));
    *event = TimerEvent {
        .time_to_exec = ms,
        .callback = callback,
        .data = data,
        .next = g_active_timer_events,
    };
    g_active_timer_events = event;

    return Success;
}

}
