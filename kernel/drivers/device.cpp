#include "device.h"


uint8_t Console::s_next_minor = 0;
uint8_t UART::s_next_minor = 0;


Device::Device(uint8_t major, uint8_t minor, char const* name)
    : m_major(major), m_minor(minor)
{
    sprintf(m_name, "%s%d", name, (int) minor);
}

int64_t default_seek_function(
    int64_t filesize, int64_t current_off,
    int64_t offset, int whence)
{
    int64_t new_offset = current_off;

    switch (whence) {
    case SEEK_CUR:
        new_offset += offset;
        break;
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_END:
        new_offset = 0;
        break;
    default:
        return -ENOTSUP;
    }

    new_offset = clamp<int64_t>(0, new_offset, filesize);

    return new_offset;
}

int32_t Console::ioctl(uint32_t, void*)
{
    return -ENOTSUP;
}
