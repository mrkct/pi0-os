#include <kernel/datetime.h>
#include <kernel/device/systimer.h>
#include <kernel/filesystem/filesystem.h>
#include <kernel/kprintf.h>

namespace kernel {

enum class DateTimeSource {
    Fake,
    File
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

static Error try_using_file_as_time_source()
{
    if (fs_get_root() == nullptr)
        return DeviceNotConnected;

    auto& fs = *fs_get_root();
    File file;
    TRY(fs_open(fs, "/DATETIME", file));

    uint8_t buf[32];
    size_t bytes_read;
    TRY(fs_read(file, buf, 0, sizeof(buf) - 1, bytes_read));
    buf[bytes_read] = '\0';

    auto const& parse_datetime_string = [](char const* string, DateTime& datetime) -> Error {
        auto const& expect_num = [](char const*& str, size_t max_digits, int& result) -> Error {
            result = 0;
            for (size_t i = 0; i < max_digits && *str; i++) {
                if (*str < '0' || *str > '9')
                    return i == 0 ? InvalidFormat : Success;
                result *= 10;
                result += *str - '0';
                ++str;
            }
            return Success;
        };

#define expect(c)       \
    if (*string++ != c) \
    return InvalidFormat

        expect_num(string, 4, datetime.year);
        expect('-');
        expect_num(string, 2, datetime.month);
        expect('-');
        expect_num(string, 2, datetime.day);
        expect(' ');
        expect_num(string, 2, datetime.hour);
        expect(':');
        expect_num(string, 2, datetime.minute);
        expect(':');
        expect_num(string, 2, datetime.second);

#undef expect

        if (datetime.month < 1 || datetime.month > 12)
            return InvalidFormat;
        if (datetime.day < 1 || datetime.day > 31)
            return InvalidFormat;
        if (datetime.hour > 23)
            return InvalidFormat;
        if (datetime.minute > 59)
            return InvalidFormat;
        if (datetime.second > 59)
            return InvalidFormat;

        return Success;
    };

    TRY(parse_datetime_string(reinterpret_cast<char const*>(buf), g_last_read_datetime));

    g_datetime_source = DateTimeSource::File;
    return Success;
}

Error datetime_init()
{
    if (!try_using_file_as_time_source().is_success()) {
        use_fake_datetime_as_time_source();
    }

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

    if (g_datetime_source == DateTimeSource::File) {
        TODO();
    }

    kassert_not_reached();
}

}
