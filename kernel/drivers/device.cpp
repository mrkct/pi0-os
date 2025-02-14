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

int64_t UART::read(uint8_t *buffer, size_t size)
{
    int64_t bytes_read = 0;

    auto lock = irq_lock();

    while (bytes_read < size && !m_rx_buffer.is_empty()) {
        m_rx_buffer.pop(buffer[bytes_read++]);
    }

    release(lock);

    return bytes_read;
}

int32_t UART::ioctl(uint32_t, void*)
{
    todo();
    return -ERR_NOTSUP;
}

int32_t UART::poll(uint32_t events, uint32_t *out_revents) const
{
    if ((events & F_POLLIN) && !m_rx_buffer.is_empty()) {
        *out_revents |= F_POLLIN;
    }

    if ((events & F_POLLOUT) && can_write()) {
        *out_revents |= F_POLLOUT;
    }

    return 0;
}

void UART::on_received(uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        m_rx_buffer.push(buffer[i]);
    }
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
    size_t elements = size / sizeof(api::InputEvent);
    api::InputEvent *out_events = (api::InputEvent*) buffer;

    for (size_t i = 0; i < elements; i++) {
        get_next_event(out_events[i]);
    }

    return elements * sizeof(api::InputEvent);
}

void InputDevice::notify_event(api::InputEvent event)
{
    mutex_take(this->m_events.lock);
    this->m_events.events.push(event);
    mutex_release(this->m_events.lock);
}

void InputDevice::get_next_event(api::InputEvent& event)
{
    while (true) {
        while (this->m_events.events.is_empty()) {
            cpu_relax();
        }
        mutex_take(this->m_events.lock);
        if (this->m_events.events.is_empty()) {
            mutex_release(this->m_events.lock);
            continue;
        }
        this->m_events.events.pop(event);
        mutex_release(this->m_events.lock);
        break;
    }
}

int32_t InputDevice::poll(uint32_t events, uint32_t *out_revents) const
{
    if ((events & F_POLLIN) && !m_events.events.is_empty()) {
        *out_revents |= F_POLLIN;
    }

    if ((events & F_POLLOUT) && !m_events.events.is_full()) {
        *out_revents |= F_POLLOUT;
    }

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
        kassert(vm_map(*as, page, vaddr + offset, PageAccessPermissions::UserFullAccess).is_success());
    }

    return 0;
}
