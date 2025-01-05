#include <stdio.h>
#include <api/syscalls.h>

#include "moretime.h"


int clock_init(Clock *clock)
{
    int fd = sys_open("/dev/rtc0", OF_RDONLY, 0);
    if (fd < 0)
        return fd;

    *clock = (Clock) {
        .fd = fd,
    };
    return 0;
}

int clock_get_datetime(Clock *clock, DateTime *datetime)
{
    return sys_ioctl(clock->fd, RTCIO_GET_DATETIME, (sysarg_t) datetime);
}
