#pragma once

#include <kernel/base.h>
#include <kernel/lib/intrusivelinkedlist.h>


typedef void (*TimerCallback)(void*);

void timer_init();

void timer_exec_once(uint64_t ms, TimerCallback callback, void *arg);

void timer_exec_periodic(uint64_t ms, TimerCallback callback, void *arg);

void timer_install_scheduler_callback(uint64_t ms, void (*callback)(InterruptFrame*));

uint64_t get_ticks();

