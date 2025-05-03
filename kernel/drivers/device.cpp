#include "device.h"

#define LOG_ENABLED
#define LOG_TAG "DEVICE"
#include <kernel/log.h>


uint8_t BlockDevice::s_next_minor = 0;
uint8_t Console::s_next_minor = 0;
uint8_t UART::s_next_minor = 0;
uint8_t GPIOController::s_next_minor = 0;
uint8_t RealTimeClock::s_next_minor = 0;
uint8_t InputDevice::s_next_minor = 0;
uint8_t FramebufferDevice::s_next_minor = 0;


FileDevice::FileDevice(uint8_t major, uint8_t minor, char const* name)
    : m_major(major), m_minor(minor)
{
    sprintf(m_name, "%s%d", name, (int) minor);
}

int64_t SimpleBlockDevice::read(int64_t offset, uint8_t *buffer, size_t size)
{
    int64_t read = 0, to_read = 0;
    int64_t rc = 0;
    uint8_t *temp_buffer = nullptr;
    int64_t sector_size = block_size();

    offset = clamp<int64_t>(0, offset, this->size());
    size = clamp<int64_t>(0, size, this->size() - offset);

    if (offset % sector_size != 0) {
        temp_buffer = (uint8_t*) malloc(sector_size);
        if (temp_buffer == nullptr) {
            rc = -ERR_NOMEM;
            goto cleanup;
        }

        rc = read_sector(offset / sector_size, temp_buffer);
        if (rc != 0)
            goto cleanup;
        
        to_read = min<int64_t>(size, sector_size - (offset % sector_size));
        memcpy(buffer, &temp_buffer[offset % sector_size], to_read);
        read += to_read;
        offset += to_read;
        buffer += to_read;
        size -= to_read;
    }

    kassert(offset % sector_size == 0);
    while (size >= sector_size) {
        rc = read_sector(offset / sector_size, buffer);
        if (rc != 0)
            goto cleanup;

        read += sector_size;
        offset += sector_size;
        buffer += sector_size;
        size -= sector_size;
    }

    kassert(size < sector_size);
    kassert(offset % sector_size == 0);
    if (size > 0) {
        if (temp_buffer == nullptr) {
            temp_buffer = (uint8_t*) malloc(sector_size);
            if (temp_buffer == nullptr) {
                rc = -ERR_NOMEM;
                goto cleanup;
            }
        }

        rc = read_sector(offset / sector_size, temp_buffer);
        if (rc != 0)
            goto cleanup;
        memcpy(buffer, temp_buffer, size);
        read += size;
        offset += size;
        buffer += size;
        size -= size;
    }

    kassert(size == 0);
    rc = read;

cleanup:
    free(temp_buffer);
    return rc;
}

int64_t SimpleBlockDevice::write(int64_t offset, const uint8_t *buffer, size_t size)
{
    if (is_read_only())
        return -ERR_ROFS;

    int64_t written = 0, to_write = 0;
    int64_t rc = 0;
    uint8_t *temp_buffer = nullptr;
    int64_t sector_size = block_size(); 

    offset = clamp<int64_t>(0, offset, this->size());
    size = clamp<int64_t>(0, size, this->size() - offset);

    if (offset % sector_size != 0) {
        temp_buffer = (uint8_t*) malloc(sector_size);
        if (temp_buffer == nullptr) {
            rc = -ERR_NOMEM;
            goto cleanup;
        }

        rc = read_sector(offset / sector_size, temp_buffer);
        if (rc != 0)
            goto cleanup;
        
        to_write = min<int64_t>(size, sector_size - (offset % sector_size));
        memcpy(&temp_buffer[offset % sector_size], buffer, to_write);

        rc = write_sector(offset / sector_size, temp_buffer);
        if (rc != 0)
            goto cleanup;

        written += to_write;
        offset += to_write;
        buffer += to_write;
        size -= to_write;
    }

    kassert(offset % sector_size == 0);
    while (size >= sector_size) {
        rc = write_sector(offset / sector_size, buffer);
        if (rc != 0)
            goto cleanup;

        written += sector_size;
        offset += sector_size;
        buffer += sector_size;
        size -= sector_size;
    }

    kassert(size < sector_size);
    kassert(offset % sector_size == 0);
    if (size > 0) {
        if (temp_buffer == nullptr) {
            temp_buffer = (uint8_t*) malloc(sector_size);
            if (temp_buffer == nullptr) {
                rc = -ERR_NOMEM;
                goto cleanup;
            }
        }

        rc = read_sector(offset / sector_size, temp_buffer);
        if (rc != 0)
            goto cleanup;
        memcpy(temp_buffer, buffer, size);

        rc = write_sector(offset / sector_size, temp_buffer);
        if (rc != 0)
            goto cleanup;

        written += size;
        offset += size;
        buffer += size;
        size -= size;
    }

    kassert(size == 0);
    rc = written;

cleanup:
    free(temp_buffer);
    return rc;
}

int32_t Console::ioctl(uint32_t, void*)
{
    todo();
    return -ERR_NOTSUP;
}

TTY::TTY(uint8_t major, uint8_t minor, const char *name):
    CharacterDevice(major, minor, name)
{
    reset_termios();
}

void TTY::reset_termios()
{
    m_termios.c_iflag = ICRNL;
    m_termios.c_oflag = ONLCR;
    m_termios.c_cflag = 0;
    m_termios.c_lflag = ICANON | ECHO | ECHOK;
}

void TTY::emit(uint8_t ch)
{
    if (ch == '\r' && m_termios.c_iflag & ICRNL) {
        ch = '\n';
    } else if (ch == '\n' && m_termios.c_iflag & INLCR) {
        ch = '\r';
    }

    if (m_termios.c_lflag & ICANON) {
        if (ch == 127) {
            if (m_linebuffer_size > 0) {
                m_linebuffer_size--;
                echo_raw('\b');
                echo_raw(' ');
                echo_raw('\b');
            }
            return;
        }

        if (m_linebuffer_size < array_size(m_linebuffer)) {
            m_linebuffer[m_linebuffer_size++] = ch;
        } else {
            // Line is full, echo a bell
            echo_raw('\a');
            return;
        }
        
        /* End of line condition */
        if (ch == '\n') {
            if (m_termios.c_lflag & ECHO || m_termios.c_lflag & ECHONL)
                echo('\n');

            flush_line();
            return;
        }
    } else {
        m_input_buffer.push(ch);
    }

    if (m_termios.c_lflag & ECHO) {
        echo(ch);
    }
}

int64_t TTY::read(uint8_t *buffer, size_t size)
{
    uint8_t *start = buffer;
    bool canon = (m_termios.c_lflag & ICANON) != 0;
    uint8_t ch;

    while ((size_t)(buffer - start) < size && m_input_buffer.pop(ch)) {
        *buffer++ = ch;
        if (canon && ch == '\n') {
            kassert(m_available_lines > 0);
            m_available_lines--;
            break;
        }
    }

    return buffer - start;
}

int64_t TTY::write(const uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        echo(buffer[i]);
    return size;
}

void TTY::echo(uint8_t ch)
{
    if ((m_termios.c_oflag & ONLCR) && ch == '\n') {
        echo_raw('\r');
    }
    echo_raw(ch);
}

int32_t TTY::ioctl(uint32_t request, void* argp)
{
    switch (request) {
        case api::TTYIO_TCGETATTR:
            *(api::termios*) argp = m_termios;
            return 0;
        case api::TTYIO_TCSETATTR:
            m_termios = *(api::termios*) argp;
            return 0;
        default:
            return -ERR_NOTSUP;
    }
}

int32_t TTY::poll(uint32_t events, uint32_t *out_revents) const
{
    bool can_read = false;
    
    if (events & F_POLLIN) {
        if (m_termios.c_lflag & ICANON) {
            can_read = m_available_lines > 0;
        } else {
            can_read = !m_input_buffer.is_empty();
        }
        if (can_read)
            *out_revents |= F_POLLIN;
    }

    if ((events & F_POLLOUT) && can_echo()) {
        *out_revents |= F_POLLOUT;
    }

    return 0;
}

void TTY::flush_line()
{
    for (size_t i = 0; i < m_linebuffer_size; i++) {
        m_input_buffer.push(m_linebuffer[i]);
    }
    m_linebuffer_size = 0;
    m_available_lines++;
    LOGD("TTY: Flushing line, m_available_lines=%lu, m_input_buffer.size()=%lu", m_available_lines, m_input_buffer.available());
}

int64_t PtyMaster::read(uint8_t *buf, size_t size)
{
    size_t n = 0;
    while (n < size && m_buf.pop(*buf++))
        n++;
    return n;
}

int64_t PtyMaster::write(const uint8_t *buf, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        m_slave.emit(buf[i]);
    }
    return size;
}

int32_t PtyMaster::poll(uint32_t events, uint32_t *out_revents) const
{
    int32_t rc = 0;
    uint32_t temp = 0;

    if ((events & F_POLLIN) && !m_buf.is_empty()) {
        *out_revents |= F_POLLIN;
    }

    if (events & F_POLLOUT) {
        rc = m_slave.poll(F_POLLOUT, &temp);
        if (rc != 0)
            return rc;
        *out_revents |= temp & F_POLLOUT;
    }

    return 0;
}

int32_t PtyMaster::ioctl(uint32_t request, void*)
{
    switch (request) {
        case api::PTYIO_GETSLAVE:
            return m_ptyid;
        default:
            return -ERR_NOTSUP;
    }
}

void PtySlave::echo_raw(uint8_t ch)
{
    m_master.m_buf.push(ch);
}

int64_t GPIOController::read(uint8_t *, size_t)
{
    todo();
    return -ERR_NOTSUP;
}

int64_t GPIOController::write(const uint8_t *, size_t)
{
    todo();
    return -ERR_NOTSUP;
}

int32_t GPIOController::ioctl(uint32_t, void *)
{
    todo();
    return -ERR_NOTSUP;
}

int32_t RealTimeClock::ioctl(uint32_t request, void *argp)
{
    int32_t rc;

    switch (request) {
        case api::RTCIO_GET_DATETIME : {
            rc = get_time(*(api::DateTime*) argp);
            break;
        }
        default: {
            rc = -ERR_NOTSUP;
            break;
        }
    }

    return rc;
}

int64_t InputDevice::read(uint8_t *buffer, size_t size)
{
    api::InputEvent *out_events = (api::InputEvent*) buffer;
    size_t i = 0;

    for (i = 0; i < size / sizeof(api::InputEvent); i++) {
        if (!get_next_event(out_events[i]))
            break;
    }

    return i * sizeof(api::InputEvent);
}

void InputDevice::notify_event(api::InputEvent event)
{
    mutex_take(this->m_events.lock);
    m_events.events.push(event);
    mutex_release(this->m_events.lock);
}

bool InputDevice::get_next_event(api::InputEvent& event)
{
    mutex_take(m_events.lock);
    bool res = m_events.events.pop(event);
    mutex_release(m_events.lock);
    return res;
}

int32_t InputDevice::poll(uint32_t events, uint32_t *out_revents) const
{
    mutex_take(m_events.lock);

    if ((events & F_POLLIN) && !m_events.events.is_empty()) {
        *out_revents |= F_POLLIN;
    }

    if ((events & F_POLLOUT) && !m_events.events.is_full()) {
        *out_revents |= F_POLLOUT;
    }
    mutex_release(m_events.lock);

    return 0;
}

int32_t InputDevice::ioctl(uint32_t request, void *argp)
{
    (void) argp;

    switch (request) {
        default:
            return -ERR_NOTSUP;
    }
}

int32_t FramebufferDevice::ioctl(uint32_t request, void *argp)
{
    (void) argp;

    switch (request) {
        case api::FBIO_GET_DISPLAY_INFO: {
            auto display_info = this->display_info();
            *reinterpret_cast<api::FramebufferDisplayInfo*>(argp) = (api::FramebufferDisplayInfo) {
                .width = display_info.width,
                .height = display_info.height,
                .pitch = display_info.pitch,
                .bytes_per_pixel = display_info.bytes_per_pixel,
            };
            return 0;
        }
        case api::FBIO_REFRESH:
            return this->refresh();
        default:
            return -ERR_NOTSUP;
    }
}

int32_t FramebufferDevice::mmap(AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t)
{
    kassert(vm_addr_is_page_aligned(vaddr));
    kassert(vm_addr_is_page_aligned(length));

    auto display_info = this->display_info();
    auto fb_phys_addr = vm_align_down_to_page(display_info.fb_phys_addr);
    auto fb_length = vm_align_up_to_page(display_info.fb_length());

    LOGD("Mapping framebuffer %s to user vaddr %p (%" PRIu32 " bytes)", name(), vaddr, fb_length);
    for (uintptr_t offset = 0; offset < fb_length; offset += _4KB) {
        PhysicalPage *page = addr2page(fb_phys_addr + offset);
        kassert(page != nullptr);
        page->ref_count++;
        kassert(vm_map(*as, page, vaddr + offset, PageAccessPermissions::UserFullAccess).is_success());
    }

    return 0;
}
