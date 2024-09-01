#ifndef API_TIME_H
#define API_TIME_H

#include <stdint.h>
#include <time.h>


typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} DateTime;

static inline void datetime_to_tm(const DateTime dt, struct tm *tm)
{
    tm->tm_year = dt.year - 1900;
    tm->tm_mon = dt.month - 1;
    tm->tm_mday = dt.day;
    tm->tm_hour = dt.hour;
    tm->tm_min = dt.minute;
    tm->tm_sec = dt.second;
}

static inline DateTime datetime_from_tm(const struct tm tm)
{
    DateTime dt;
    dt.year = tm.tm_year + 1900;
    dt.month = tm.tm_mon + 1;
    dt.day = tm.tm_mday;
    dt.hour = tm.tm_hour;
    dt.minute = tm.tm_min;
    dt.second = tm.tm_sec;
    return dt;
}

#endif