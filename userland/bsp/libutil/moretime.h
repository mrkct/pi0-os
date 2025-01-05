#ifndef MORETIME_H
#define MORETIME_H



typedef struct Clock {
    int fd;
} Clock;

int clock_init(Clock *clock);

int clock_get_datetime(Clock *clock, DateTime *datetime);

#endif
