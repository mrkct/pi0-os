#ifndef API_TIME_H
#define API_TIME_H

#include <stdint.h>


typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    uint64_t ticks_since_boot;
} DateTime;

#endif