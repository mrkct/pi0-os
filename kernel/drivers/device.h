#pragma once

#include <stdio.h>
#include <kernel/base.h>
#include <kernel/irq.h>
#include <kernel/memory/vm.h>
#include <kernel/arch/arch.h>
#include <kernel/lib/ringbuffer.h>
#include <kernel/locking/mutex.h>
#include <include/api/syscalls.h>
#include <include/api/input.h>
#include <sys/termios.h>


class Device
{
public:
    enum class Type {
        CharacterDevice,
        BlockDevice,
        InterruptController,
        SystemTimer,
    };
    virtual ~Device() {}

    virtual bool is_mountable() const { return false; }
    virtual const char *name() const = 0;
    virtual Device::Type device_type() const = 0;
    virtual int32_t init_for_early_boot() { return -ERR_NOTSUP; }
    virtual int32_t init() = 0;
    virtual int32_t shutdown() = 0;
};

class InterruptController: public Device
{
public:
    virtual ~InterruptController() {}

    virtual Device::Type device_type() const override { return Device::Type::InterruptController; }
    virtual int32_t shutdown() override { panic("You can't shutdown the interrupt controller!"); }

    virtual void mask_interrupt(uint32_t irqidx) = 0;
    virtual void unmask_interrupt(uint32_t irqidx) = 0;
    virtual void install_irq(uint32_t irqidx, InterruptHandler handler, void *arg) = 0;
    virtual void dispatch_irq(InterruptFrame *frame) = 0;
};

class SystemTimer: public Device
{
public:
    virtual ~SystemTimer() {}

    virtual Device::Type device_type() const override { return Device::Type::SystemTimer; }
    virtual int32_t shutdown() override { panic("You can't shutdown the system timer!"); }

    typedef void (*SystemTimerCallback)(InterruptFrame*, SystemTimer&, uint64_t, void*);

    virtual uint64_t ticks() const = 0;
    virtual uint64_t ticks_per_ms() const = 0;
    virtual void start(uint64_t ticks, SystemTimerCallback, void *arg) = 0;
};

class FileDevice: public Device
{
public:
    FileDevice(uint8_t major, uint8_t minor, const char *name);
    virtual ~FileDevice() {}

    virtual bool is_mountable() const override final { return true; }
    uint16_t dev_id() const { return (m_major << 8) | m_minor; }
    virtual uint8_t major() const { return m_major; }
    virtual uint8_t minor() const { return m_minor; }
    virtual const char *name() const override { return m_name; }

    virtual int64_t read(int64_t, uint8_t *buffer, size_t size) = 0;
    virtual int64_t write(int64_t, const uint8_t *buffer, size_t size) = 0;
    virtual int32_t ioctl(uint32_t request, void *argp) = 0;
    virtual int32_t poll(uint32_t events, uint32_t *out_revents) const {
        *out_revents = events & F_POLLMASK;
        return 0;
    }
    virtual bool is_tty() const { return false; }
    virtual int32_t mmap(AddressSpace*, uintptr_t, uint32_t, uint32_t) { return -ERR_NOTSUP; }

private:
    uint8_t m_major, m_minor;
    char m_name[32];
};

class CharacterDevice: public FileDevice
{
public:
    CharacterDevice(uint8_t major, uint8_t minor, const char *name)
        : FileDevice(major, minor, name)
    {}
    virtual ~CharacterDevice() {}

    virtual Device::Type device_type() const override { return Device::Type::CharacterDevice; }

    virtual int64_t read(int64_t, uint8_t *buffer, size_t size) override { return read(buffer, size); }
    virtual int64_t read(uint8_t *buffer, size_t size) = 0;

    virtual int64_t write(int64_t, const uint8_t *buffer, size_t size) override { return write(buffer, size); }
    virtual int64_t write(const uint8_t *buffer, size_t size) = 0;
};

class BlockDevice: public FileDevice
{
private:
    static uint8_t s_next_minor;
public:
    BlockDevice(int64_t block_size, const char *name)
        : FileDevice(Maj_Disk, s_next_minor++, name), m_block_size(block_size)
    {}
    virtual ~BlockDevice() {}

    virtual int64_t block_size() const { return m_block_size; }
    virtual Device::Type device_type() const override { return Device::Type::BlockDevice; }
    virtual uint64_t size() const = 0;

private:
    int64_t m_block_size;
};

class SimpleBlockDevice: public BlockDevice
{
public:
    SimpleBlockDevice(int64_t block_size, const char *name)
        : BlockDevice(block_size, name)
    {}
    virtual ~SimpleBlockDevice() {}
    
    virtual int64_t read(int64_t, uint8_t *buffer, size_t size) override;
    virtual int64_t write(int64_t, const uint8_t *buffer, size_t size) override;

protected:
    virtual int64_t read_sector(int64_t sector_idx, uint8_t *buffer) = 0;
    virtual int64_t write_sector(int64_t sector_idx, uint8_t const *buffer) = 0;
    virtual bool is_read_only() const { return false; }

private:
    uint8_t *m_temp_buffer;
};

class Console: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    Console(): CharacterDevice(Maj_Console, s_next_minor++, "console")
    {
    }
    virtual ~Console() {}

    virtual int64_t read(uint8_t*, size_t) override { return -ERR_NOTSUP; }
    virtual int32_t ioctl(uint32_t request, void *argp) override;

protected:
    virtual int32_t get_console_size(size_t&, size_t&) const { return -ERR_NOTSUP; }
    virtual int32_t set_console_size(size_t, size_t) { return -ERR_NOTSUP; }
    virtual int32_t clear_screen() const { return -ERR_NOTSUP; }
    virtual int32_t get_cursorr_position(size_t&, size_t&) const { return -ERR_NOTSUP; }
    virtual int32_t set_cursor_position(size_t, size_t) { return -ERR_NOTSUP; }
};

class TTY: public CharacterDevice
{
public:
    TTY(uint8_t major, uint8_t minor, const char *name);
    virtual ~TTY() {}

    virtual int64_t read(uint8_t* buffer, size_t size) override;
    virtual int64_t write(const uint8_t* buffer, size_t size) override;
    virtual int32_t ioctl(uint32_t request, void *argp) override;
    virtual int32_t poll(uint32_t events, uint32_t *out_revents) const override;
    virtual bool is_tty() const override { return true; }

protected:
    void reset_termios();
    void flush_line();
    virtual void echo_raw(uint8_t ch) = 0;
    void echo(uint8_t ch);
    virtual bool can_echo() const { return true; }
    void emit(uint8_t c);

    struct termios m_termios;
    uint8_t m_linebuffer[256];
    size_t m_linebuffer_size;
    size_t m_available_lines { 0 };
    RingBuffer<4096, uint8_t> m_input_buffer;
};

class UART: public TTY
{
private:
    static uint8_t s_next_minor;
public:
    UART(): TTY(Maj_UART, s_next_minor++, "ttyS") {}
    virtual ~UART() {}
};

class GPIOController: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    GPIOController(): CharacterDevice(Maj_GPIO, s_next_minor++, "gpio")
    {}
    virtual ~GPIOController() {}

    enum class PinState { Low, High };
    enum class PullState { None, Up, Down };
    enum class PinFunction {
        Input,
        Output,
        Alt0,
        Alt1,
        Alt2,
        Alt3,
        Alt4,
        Alt5,
    };

    virtual int64_t read(uint8_t *buffer, size_t size) override;
    virtual int64_t write(const uint8_t *buffer, size_t size) override;
    virtual int32_t ioctl(uint32_t request, void *argp) override;

    virtual int32_t configure_pin(uint32_t port, uint32_t pin, PinFunction function) = 0;
    virtual int32_t configure_pin_pull_up_down(uint32_t port, uint32_t pin, PullState) = 0;


    virtual int32_t get_port_count() = 0;
    virtual int32_t get_port_pin_count(uint32_t port) = 0;
    virtual int32_t get_pin_state(uint32_t port, uint32_t pin) = 0;
    virtual int32_t set_pin_state(uint32_t port, uint32_t pin, PinState state) = 0;
};

class RealTimeClock: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    RealTimeClock(): CharacterDevice(Maj_RTC, s_next_minor++, "rtc")
    {}
    virtual ~RealTimeClock() {}

    virtual int64_t read(uint8_t*, size_t) override { return -ERR_NOTSUP; }
    virtual int64_t write(const uint8_t*, size_t) override { return -ERR_NOTSUP; }
    virtual int32_t ioctl(uint32_t request, void *argp) override;

    virtual int32_t get_time(api::DateTime&) = 0;
    virtual int32_t set_time(const api::DateTime) = 0;
};

class InputDevice: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    InputDevice()
        : CharacterDevice(Maj_Input, s_next_minor++, "input")
    {
        mutex_init(this->m_events.lock, MutexInitialState::Unlocked);
    }

    virtual ~InputDevice() {};

    virtual int64_t read(uint8_t *buffer, size_t size) override;
    virtual int64_t write(const uint8_t*, size_t) override { return -ERR_NOTSUP; }
    virtual int32_t ioctl(uint32_t request, void *argp) override;
    virtual int32_t poll(uint32_t events, uint32_t *out_revents) const override;

protected:
    void notify_event(api::InputEvent);

private:
    void get_next_event(api::InputEvent&);

    struct {
        Mutex lock;
        RingBuffer<32, api::InputEvent> events;
    } m_events;
};

class FramebufferDevice: public CharacterDevice
{
private:
    static uint8_t s_next_minor;
public:
    FramebufferDevice():
        CharacterDevice(Maj_Framebuffer, s_next_minor++, "framebuffer")
    {}

    struct DisplayInfo {
        uint32_t width, height, bytes_per_pixel, pitch;
        uintptr_t fb_phys_addr;

        constexpr uint32_t fb_length() const { return pitch * height; }
    };

    virtual ~FramebufferDevice() {}

    virtual DisplayInfo display_info() const = 0;
    virtual int32_t refresh() = 0;

    virtual int64_t read(uint8_t*, size_t) override { return -ERR_NOTSUP; }
    virtual int64_t write(const uint8_t*, size_t) override { return -ERR_NOTSUP; }
    virtual int32_t ioctl(uint32_t request, void *argp) override;
    virtual int32_t mmap(AddressSpace*, uintptr_t, uint32_t, uint32_t) override;
};
