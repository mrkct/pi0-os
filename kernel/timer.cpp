#include <kernel/drivers/devicemanager.h>
#include <kernel/memory/kheap.h>
#include <kernel/locking/irqlock.h>

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
static struct {
    void (*callback)(InterruptFrame*) = [](InterruptFrame*) {};
    uint64_t next_deadline = 0;
    uint64_t period = 0;
} s_scheduler;

void timer_init()
{
    auto *systimer = devicemanager_get_system_timer_device();
    kassert(systimer != nullptr);

    uint64_t period = 5 * systimer->ticks_per_ms();
    systimer->start(period, [](InterruptFrame *iframe, SystemTimer &systimer, uint64_t, void*) {
        s_timers.foreach([&](Timer *timer) {
            if (timer->start_time + timer->period <= systimer.ticks()) {
                timer->callback(timer->arg);
                
                if (timer->type == TimerType::OneShot) {
                    s_timers.remove(timer);
                    kfree(timer);
                } else {
                    timer->start_time = systimer.ticks();
                }
            }
        });

        if (s_scheduler.next_deadline <= systimer.ticks()) {
            s_scheduler.callback(iframe);
            s_scheduler.next_deadline = systimer.ticks() + s_scheduler.period;
        }
    }, nullptr);
}

static void schedule_timer(Timer *timer, uint64_t ms)
{
    auto *systimer = devicemanager_get_system_timer_device();
    kassert(systimer != nullptr);

    auto lock = irq_lock();
    {
        timer->start_time = systimer->ticks();
        timer->period = ms * systimer->ticks_per_ms();
        s_timers.add(timer);
    }
    release(lock);
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

void timer_install_scheduler_callback(uint64_t ms, void (*callback)(InterruptFrame*))
{
    auto *systimer = devicemanager_get_system_timer_device();
    kassert(systimer != nullptr);

    auto lock = irq_lock();
    {
        s_scheduler.period = systimer->ticks_per_ms() * ms;
        s_scheduler.next_deadline = systimer->ticks() + s_scheduler.period;
        s_scheduler.callback = callback;
    }
    release(lock);
}

uint64_t get_ticks()
{
    auto *systimer = devicemanager_get_system_timer_device();
    if (systimer == nullptr)
        return 0;
    return systimer->ticks();
}

uint32_t get_ticks_ms()
{
    auto *systimer = devicemanager_get_system_timer_device();
    if (systimer == nullptr)
        return 0;
    return (uint32_t) (systimer->ticks() / systimer->ticks_per_ms());
}
