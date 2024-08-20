#pragma once

#include <stdio.h>
#include <kernel/base.h>
#include <kernel/memory/vm.h>
#include <kernel/arch/arm/io.h>
#include <include/api/syscalls.h>


int64_t default_seek_function(int64_t offset, int whence, int64_t filesize);

class Device
{
public:
    enum class Type {
        CharacterDevice,
        BlockDevice
    };

    Device(uint8_t major, uint8_t minor, const char *name);

    virtual uint8_t major() const { return m_major; }
    virtual uint8_t minor() const { return m_minor; }
    virtual const char *name() const { return m_name; }
    virtual Type type() const = 0;

    virtual int32_t init() = 0;
    virtual int32_t shutdown() = 0;

    virtual int64_t read(int64_t offset, uint8_t *buffer, size_t size) = 0;
    virtual int64_t write(int64_t offset, const uint8_t *buffer, size_t size) = 0;
    virtual int64_t seek(int64_t offset, int whence) = 0;
    virtual int32_t ioctl(uint32_t request, void *argp) = 0;

protected:
    int64_t m_seekoff = 0;

private:
    uint8_t m_major, m_minor;
    char m_name[32];
};

class CharacterDevice: public Device
{
public:
    CharacterDevice(uint8_t major, uint8_t minor, const char *name)
        : Device(major, minor, name)
    {}

    virtual Type type() const override { return Type::CharacterDevice; }

    virtual int64_t read(int64_t, uint8_t *buffer, size_t size) override { return read(buffer, size); }
    virtual int64_t read(uint8_t *buffer, size_t size) = 0;

    virtual int64_t write(int64_t, const uint8_t *buffer, size_t size) override { return write(buffer, size); }
    virtual int64_t write(const uint8_t *buffer, size_t size) = 0;

    virtual int64_t seek(int64_t, int) override { return 0; }
};

class BlockDevice: public Device
{
public:
    BlockDevice(uint8_t major, uint8_t minor, const char *name)
        : Device(major, minor, name)
    {}

    virtual Type type() const override { return Type::BlockDevice; }
};

class Console: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    Console(): CharacterDevice(Maj_Console, s_next_minor++, "console")
    {
    }

    virtual int64_t read(uint8_t*, size_t) override { return -ENOTSUP; }
    virtual int32_t ioctl(uint32_t request, void *argp) override;

protected:
    virtual int32_t get_console_size(size_t&, size_t&) const { return -ENOTSUP; }
    virtual int32_t set_console_size(size_t, size_t) { return -ENOTSUP; }
    virtual int32_t clear_screen() const { return -ENOTSUP; }
    virtual int32_t get_cursorr_position(size_t&, size_t&) const { return -ENOTSUP; }
    virtual int32_t set_cursor_position(size_t, size_t) { return -ENOTSUP; }
};

class UART: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    UART(): CharacterDevice(Maj_UART, s_next_minor++, "uart")
    {}
};
