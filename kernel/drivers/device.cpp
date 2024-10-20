#include "device.h"


uint8_t BlockDevice::s_next_minor = 0;
uint8_t Console::s_next_minor = 0;
uint8_t UART::s_next_minor = 0;
uint8_t GPIOController::s_next_minor = 0;
uint8_t RealTimeClock::s_next_minor = 0;


FileDevice::FileDevice(uint8_t major, uint8_t minor, char const* name)
    : m_major(major), m_minor(minor)
{
    sprintf(m_name, "%s%d", name, (int) minor);
}

int32_t Console::ioctl(uint32_t, void*)
{
    todo();
    return -ENOTSUP;
}

int32_t UART::ioctl(uint32_t, void*)
{
    todo();
    return -ENOTSUP;
}

int64_t GPIOController::read(uint8_t *, size_t)
{
    todo();
    return -ENOTSUP;
}

int64_t GPIOController::write(const uint8_t *, size_t)
{
    todo();
    return -ENOTSUP;
}

int32_t GPIOController::ioctl(uint32_t, void *)
{
    todo();
    return -ENOTSUP;
}

int32_t RealTimeClock::ioctl(uint32_t request, void*)
{
    int32_t rc;

    switch (request) {
        default: {
            rc = -ENOTSUP;
            break;
        }
    }

    return rc;
}
