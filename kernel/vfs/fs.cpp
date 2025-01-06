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

int32_t fs_inode_ioctl_not_supported(Inode*, uint32_t, void*)
{
    return -ERR_NOTSUP;
}

uint64_t fs_inode_seek_not_supported(Inode*, uint64_t, int, int32_t)
{
    return -ERR_NOTSUP;
}

int32_t fs_file_inode_poll_always_ready(Inode*, uint32_t events, uint32_t *out_revents)
{
    *out_revents = events & F_POLLMASK;
    return 0;
}

int32_t fs_file_inode_mmap_not_supported(Inode*, AddressSpace*, uintptr_t, uint32_t, uint32_t)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_create_not_supported(Inode*, const char*, InodeType, Inode **)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_mkdir_not_supported(Inode*, const char *)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_rmdir_not_supported(Inode*, const char *)
{
    return -ERR_NOTSUP;
}

int fs_dir_inode_unlink_not_supported(Inode*, const char *)
{
    return -ERR_NOTSUP;
}

