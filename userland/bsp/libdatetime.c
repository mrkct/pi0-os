#include <api/syscalls.h>
#include "libdatetime.h"


DateTime datetime_add(DateTime dt, int hours_to_add, int minutes_to_add, int seconds_to_add)
{
    dt.second += seconds_to_add;
    if (dt.second >= 60) {
        dt.minute += dt.second / 60;
        dt.second = dt.second % 60;
    }

    dt.minute += minutes_to_add;
    if (dt.minute >= 60) {
        dt.hour += dt.minute / 60;
        dt.minute = dt.minute % 60;
    }

    dt.hour += hours_to_add;
    if (dt.hour >= 24) {
        dt.day += dt.hour / 60;
        dt.hour = dt.hour % 24;
    }

    // TODO: Handle days/months wrap-around

    return dt;
}
