#ifndef MORE_TIME_H
#define MORE_TIME_H

// NOTE: This file is written in C because I'd like to port it to the userland later

#include <time.h>

int secs_to_tm(long long t, struct tm *tm);

long long tm_to_secs(const struct tm *tm);


#endif
