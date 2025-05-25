#include <kernel/base.h>
#include <kernel/drivers/devicemanager.h>
#include "devfs.h"

#define LOG_ENABLED
#define LOG_TAG "DEVFS"
#include <kernel/log.h>


enum class MountableDeviceType {
    CharacterDevice,
    BlockDevice,
};

struct DevFSFilesystemCtx {
};

struct DevFSInodeCtx {
    MountableDeviceType type;
    FileDevice *device;
};

static int devfs_fs_on_mount(Filesystem *self, Inode *out_root);
static int devfs_fs_open_inode(Filesystem*, Inode*);
static int devfs_fs_close_inode(Filesystem*, Inode*);

static int64_t devfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
static int64_t devfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
static int32_t devfs_file_inode_ioctl(Inode *self, uint32_t request, void *argp);
static uint64_t devfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset);
static int32_t devfs_file_inode_poll(Inode *self, uint32_t events, uint32_t *out_revents);
static int32_t devfs_file_inode_mmap(Inode*, AddressSpace*, uintptr_t, uint32_t, uint32_t);
static int32_t devfs_file_inode_istty(Inode*);

static int devfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode);
static int devfs_dir_inode_create(Inode *self, const char *name, InodeType type, Inode *out_inode);
static int devfs_dir_inode_unlink(Inode *self, const char *name);
static int64_t devfs_dir_inode_getdents(Inode *self, int64_t offset, struct dirent *entries, size_t count);


static struct FilesystemOps s_devfs_ops {
    .on_mount = devfs_fs_on_mount,
    .open_inode = devfs_fs_open_inode,
    .close_inode = devfs_fs_close_inode,
};

static struct InodeOps s_devfs_inode_ops {
};

static struct InodeFileOps s_devfs_inode_file_ops {
    .read = devfs_file_inode_read,
    .write = devfs_file_inode_write,
    .seek = devfs_file_inode_seek,
    .ioctl = devfs_file_inode_ioctl,
    .poll = devfs_file_inode_poll,
    .mmap = devfs_file_inode_mmap,
    .istty = devfs_file_inode_istty,
};

static struct InodeDirOps s_devfs_inode_dir_ops {
    .lookup = devfs_dir_inode_lookup,
    .create = devfs_dir_inode_create,
    .unlink = devfs_dir_inode_unlink,
    .getdents = devfs_dir_inode_getdents,
};

static InodeType device_type_to_inode_type(Device::Type type)
{
    switch (type) {
        case Device::Type::CharacterDevice:
            return InodeType::CharacterDevice;
        case Device::Type::BlockDevice:
            return InodeType::BlockDevice;
        default:
            panic("Cannot convert device type %d to any inode type", type);
    }
}

static uint64_t get_device_size(FileDevice const *device)
{
    switch (device->device_type()) {
        case Device::Type::CharacterDevice:
            return 0;
        case Device::Type::BlockDevice:
            return reinterpret_cast<BlockDevice const *>(device)->size();
        default:
            panic("Cannot get size of device type %d", device->device_type());
    }
}

static FileDevice *get_device_by_name(const char *name)
{
    Device *generic_device = devicemanager_get_by_name(name);
    if (generic_device == nullptr) {
        LOGE("Could not find device %s", name);
        return nullptr;
    } else if (!generic_device->is_mountable()) {
        LOGE("Device %s cannot be mounted", name);
        return nullptr;
    }

    return reinterpret_cast<FileDevice*>(generic_device);
}

static int devfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode)
{
    int rc = 0;
    FileDevice *device = nullptr;
    LOGI("Looking up %s in inode %" PRIu64, name, self->identifier);

    if (self->identifier != 0) {
        LOGE("Lookups in devfs are only allowed in inode 0");
        rc = -ERR_NOENT;
        goto failed;
    }

    device = get_device_by_name(name);
    if (device == nullptr) {
        LOGE("Could not find device %s", name);
        rc = -ERR_NOENT;
        goto failed;
    }

    LOGI("Found device %s", name);

    *out_inode = (Inode) {
        .refcount = 1,
        .type = device_type_to_inode_type(device->device_type()),
        .identifier = device->dev_id(),
        .filesystem = self->filesystem,
        .devmajor = 0,
        .devminor = 0,
        .mode = 0666,
        .uid = 0,
        .gid = 0,
        .size = get_device_size(device),
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .blksize = 512,
        .opaque = nullptr,
        .ops = &s_devfs_inode_ops,
        .file_ops = &s_devfs_inode_file_ops,
    };

    return 0;

failed:
    return rc;
}

static int devfs_dir_inode_create(Inode*, const char*, InodeType, Inode*)
{
    return -ERR_NOTSUP;
}

static int devfs_dir_inode_unlink(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int64_t devfs_dir_inode_getdents(Inode*, int64_t, struct dirent*, size_t)
{
    /* TODO: Actually implement this */
    return 0;
}

static int64_t devfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx*) self->opaque;
    return ctx->device->read(offset, buffer, size);
}

static int64_t devfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx*) self->opaque;
    return ctx->device->write(offset, buffer, size);
}

static int32_t devfs_file_inode_ioctl(Inode *self, uint32_t ioctl, void *argp)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx*) self->opaque;
    return ctx->device->ioctl(ioctl, argp);
}

static uint64_t devfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx *) self->opaque;
    switch (ctx->type) {
        case MountableDeviceType::CharacterDevice:
            return 0;

        case MountableDeviceType::BlockDevice: {
            BlockDevice *device = reinterpret_cast<BlockDevice *>(ctx->device);
            return default_checked_seek(device->size(), current, whence, offset);
        }

        default:
            panic("Unknown device type %d", ctx->type);
    }
}

static int32_t devfs_file_inode_poll(Inode *self, uint32_t events, uint32_t *out_revents)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx*) self->opaque;
    return ctx->device->poll(events, out_revents);
}

static int32_t devfs_file_inode_mmap(Inode *self, AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t flags)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx*) self->opaque;
    return ctx->device->mmap(as, vaddr, length, flags);
}

static int32_t devfs_file_inode_istty(Inode *self)
{
    DevFSInodeCtx *ctx = (DevFSInodeCtx*) self->opaque;
    return ctx->device->is_tty();
}

static int devfs_fs_on_mount(Filesystem *self, Inode *out_root)
{
    *out_root = (Inode) {
        .refcount = 0,
        .type = InodeType::Directory,
        .identifier = 0,
        .filesystem = self,
        .devmajor = 0,
        .devminor = 0,
        .mode = 0,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .blksize = 512,
        .opaque = nullptr,
        .ops = &s_devfs_inode_ops,
        .dir_ops = &s_devfs_inode_dir_ops,
    };

    return 0;
}

static int devfs_fs_open_inode(Filesystem*, Inode *inode)
{
    int rc;
    DevFSInodeCtx *ctx = nullptr;
    FileDevice *device = nullptr;

    if (inode->identifier == 0) {
        LOGI("Opening the root inode");
        inode->opaque = nullptr;
        return 0;
    }

    device = (FileDevice*) devicemanager_get_by_devid(inode->identifier);
    if (device == nullptr) {
        LOGE("Could not find device with devid %" PRIu64, inode->identifier);
        LOGE("This is a bug, there shouldn't be inodes in DevFS with invalid identifiers");
        rc = -ERR_NOENT;
        goto failed;
    } else if (!device->is_mountable()) {
        LOGE("Device %s is not mountable, but it should be", device->name());
        LOGE("This is a bug, there shouldn't be inodes in DevFS for devices that are not mountable");
        rc = -ERR_INVAL;
        goto failed;
    }

    LOGI("Opening inode %" PRIu64 " (device '%s')", inode->identifier, device->name());
    ctx = static_cast<DevFSInodeCtx*>(malloc(sizeof(DevFSInodeCtx)));
    if (ctx == nullptr) {
        LOGE("Could not allocate inode context");
        rc = -ERR_NOMEM;
        goto failed;
    }
    *ctx = DevFSInodeCtx {
        .type = device->device_type() == Device::Type::CharacterDevice ? MountableDeviceType::CharacterDevice : MountableDeviceType::BlockDevice,
        .device = device,
    };
    inode->opaque = ctx;

    return 0;
failed:
    return rc;
}

static int devfs_fs_close_inode(Filesystem*, Inode *inode)
{
    // TODO: Call 'flush' on the device
    free(inode->opaque);
    inode->opaque = nullptr;
    return 0;
}

int devfs_create(Filesystem **out_fs)
{
    int rc = 0;
    uint8_t *mem;
    DevFSFilesystemCtx *ctx = nullptr;
    
    mem = (uint8_t*) malloc(sizeof(Filesystem) + sizeof(DevFSFilesystemCtx));
    if (!mem) {
        rc = -ERR_NOMEM;
        goto failed;
    }

    *out_fs = (Filesystem*) mem;
    ctx = (DevFSFilesystemCtx*) &mem[sizeof(Filesystem)];

    *ctx = DevFSFilesystemCtx {};
    **out_fs = (Filesystem) {
        .ops = &s_devfs_ops,
        .icache = {},
        .root = 0,
        .opaque = ctx,
    };

    LOGI("Created devfs");

    return 0;
failed:
    return rc;
}

