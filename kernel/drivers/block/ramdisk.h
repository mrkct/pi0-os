#pragma once

#include <kernel/drivers/device.h>


class RamDisk: public BlockDevice
{
public:
    RamDisk(uint8_t *start, int64_t size, bool readonly):
        BlockDevice("ram"),
        m_start(start), m_size(size), m_readonly(readonly) {}

    virtual int32_t init() override { return 0; }
    virtual int32_t shutdown() override { return 0; }

    virtual int64_t read(int64_t offset, uint8_t *buffer, size_t size) override;
    virtual int64_t write(int64_t offset, const uint8_t *buffer, size_t size) override;
    virtual int32_t ioctl(uint32_t request, void *argp) override;
    virtual uint64_t size() const override { return m_size; }

private:
    uint8_t *m_start;
    int64_t m_size;
    bool m_readonly;
};
