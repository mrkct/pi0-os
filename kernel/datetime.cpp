#include <kernel/datetime.h>
#include <kernel/device/systimer.h>
#include <kernel/kprintf.h>

namespace kernel {

enum class DateTimeSource {
    Fake,
};

static DateTimeSource g_datetime_source = DateTimeSource::Fake;
static DateTime g_last_read_datetime;

static void use_fake_datetime_as_time_source()
{
    g_datetime_source = DateTimeSource::Fake;
    g_last_read_datetime.year = 2023;
    g_last_read_datetime.month = 7;
    g_last_read_datetime.day = 25;
    g_last_read_datetime.hour = 9;
    g_last_read_datetime.minute = 45;
    g_last_read_datetime.second = 23;
}

Error datetime_init()
{
    use_fake_datetime_as_time_source();

    return Success;
}

Error datetime_read(DateTime& datetime)
{
    datetime = g_last_read_datetime;
    datetime.ticks_since_boot = systimer_get_ticks();

    return Success;
}

Error datetime_set(DateTime& datetime)
{
    if (g_datetime_source == DateTimeSource::Fake) {
        g_last_read_datetime = datetime;
        return Success;
    }

    kassert_not_reached();
}

}
