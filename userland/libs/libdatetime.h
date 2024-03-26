#pragma once

#include <api/syscalls.h>

void os_sleep(int ms);

int get_datetime(DateTime*);

DateTime datetime_add(DateTime, int hours_to_add, int minutes_to_add, int seconds_to_add);
