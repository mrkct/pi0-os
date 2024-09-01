#include <kernel/drivers/devicemanager.h>
#include <kernel/memory/kheap.h>

#include "timer.h"


enum class TimerType {
    Periodic,
    OneShot,
};

struct Timer {
    INTRUSIVE_LINKED_LIST_HEADER(Timer);

    TimerType type;
    TimerCallback callback;
    void *arg;

    uint64_t start_time;
    uint64_t period;
};


static IntrusiveLinkedList<Timer> s_timers;

void timer_init()
{
    auto *systimer = devicemanager_get_system_timer_device();
    kassert(systimer != nullptr);

    uint64_t period = 5 * systimer->ticks_per_ms();
    systimer->start(period, [](InterruptFrame*, SystemTimer &systimer, uint64_t, void*) {
        s_timers.foreach([&](Timer *timer) {
            if (timer->start_time + timer->period <= systimer.ticks()) {
                timer->callback(timer->arg);
                
                if (timer->type == TimerType::OneShot) {
                    s_timers.remove(timer);
                    kfree(&timer);
                } else {
                    timer->start_time = systimer.ticks();
                }
            }
        });
    }, nullptr);
}

static void schedule_timer(Timer *timer, uint64_t ms)
{
    auto *systimer = devicemanager_get_system_timer_device();
    kassert(systimer != nullptr);

    timer->start_time = systimer->ticks();
    timer->period = ms * systimer->ticks_per_ms();
    s_timers.add(timer);
}

void timer_exec_once(uint64_t ms, TimerCallback callback, void *arg)
{
    Timer *timer = reinterpret_cast<Timer*>(mustmalloc(sizeof(Timer)));
    timer->type = TimerType::OneShot;
    timer->callback = callback;
    timer->arg = arg;

    schedule_timer(timer, ms);
}

void timer_exec_periodic(uint64_t ms, TimerCallback callback, void *arg)
{
    Timer *timer = reinterpret_cast<Timer*>(mustmalloc(sizeof(Timer)));
    timer->type = TimerType::Periodic;
    timer->callback = callback;
    timer->arg = arg;
    
    schedule_timer(timer, ms);
}
