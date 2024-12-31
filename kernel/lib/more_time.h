#ifndef MORE_TIME_H
#define MORE_TIME_H

// NOTE: This file is written in C because I'd like to port it to the userland later

#include <api/syscalls.h>
#include <time.h>

int secs_to_tm(long long t, struct tm *tm);

long long tm_to_secs(const struct tm *tm);

static inline void datetime_to_tm(const api::DateTime dt, struct tm *tm)
{
    tm->tm_year = dt.year - 1900;
    tm->tm_mon = dt.month - 1;
    tm->tm_mday = dt.day;
    tm->tm_hour = dt.hour;
    tm->tm_min = dt.minute;
    tm->tm_sec = dt.second;
}

static inline api::DateTime datetime_from_tm(const struct tm tm)
{
    api::DateTime dt;
    dt.year = tm.tm_year + 1900;
    dt.month = tm.tm_mon + 1;
    dt.day = tm.tm_mday;
    dt.hour = tm.tm_hour;
    dt.minute = tm.tm_min;
    dt.second = tm.tm_sec;
    return dt;
}

#endif
