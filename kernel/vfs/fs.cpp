#include "fs.h"
#include "fat32/fat32.h"

#define LOG_ENABLED
#define LOG_TAG "FS"
#include <kernel/log.h>


uint64_t default_checked_seek(uint64_t filesize, uint64_t current, int whence, int32_t offset)
{
    // FIXME: This doesn't handle overflow very well
    uint64_t newoff;
    switch (whence) {
    case SEEK_SET:
        newoff = offset;
        break;
    case SEEK_CUR:
        newoff = current + offset;
        break;
    case SEEK_END:
        newoff = ((int64_t) filesize) - offset;
        break;
    }
    return clamp<uint64_t>(0, newoff, filesize);
}

Filesystem *fs_detect_and_create(BlockDevice &device)
{
    int rc;
    Filesystem *fs = nullptr;
    
    rc = fat32_try_create(device, &fs);
    if (rc == 0) {
        LOGI("Device '%s' is detected as FAT32", device.name());
        return fs;
    } else if (rc < 0 && rc != -ERR_INVAL) {
        LOGE("Device '%s' is detected as FAT32, but failed to mount: %d", device.name(), rc);
        return nullptr;
    }

    LOGE("Failed to find a compatible filesystem on device '%s'", device.name());
    return nullptr;
}
