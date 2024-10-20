#include "ramdisk.h"


int64_t RamDisk::read(int64_t offset, uint8_t *buffer, size_t size)
{
    auto to_read = min<int64_t>(m_size - offset, size);
    memcpy(buffer, m_start + offset, to_read);
    return to_read;
}

int64_t RamDisk::write(int64_t offset, const uint8_t *buffer, size_t size)
{
    if (m_readonly)
        return -EPERM;
    
    auto to_write = min<int64_t>(m_size - offset, size);
    memcpy(m_start + offset, buffer, to_write);
    return to_write;
}

int32_t RamDisk::ioctl(uint32_t, void*)
{
    return -ENOTSUP;
}
