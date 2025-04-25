#include <kernel/base.h>
#include <kernel/drivers/devicemanager.h>
#include "ttyfs.h"

#define LOG_ENABLED
#define LOG_TAG "TTYFS"
#include <kernel/log.h>


struct TTYFSFilesystemCtx {
};

struct TTYFSInodeCtx {
};

static int ttyfs_fs_on_mount(Filesystem *self, Inode *out_root);
static int ttyfs_fs_open_inode(Filesystem*, Inode*);
static int ttyfs_fs_close_inode(Filesystem*, Inode*);

static int ttyfs_inode_stat(Inode *self, api::Stat *st);

static int64_t ttyfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
static int64_t ttyfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
static int32_t ttyfs_file_inode_ioctl(Inode *self, uint32_t request, void *argp);
static uint64_t ttyfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset);
static int32_t ttyfs_file_inode_poll(Inode *self, uint32_t events, uint32_t *out_revents);
static int32_t ttyfs_file_inode_mmap(Inode*, AddressSpace*, uintptr_t, uint32_t, uint32_t);

static int ttyfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode);
static int ttyfs_dir_inode_create(Inode *self, const char *name, InodeType type, Inode **out_inode);
static int ttyfs_dir_inode_mkdir(Inode *self, const char *name);
static int ttyfs_dir_inode_rmdir(Inode *self, const char *name);
static int ttyfs_dir_inode_unlink(Inode *self, const char *name);


static struct FilesystemOps s_ttyfs_ops {
    .on_mount = ttyfs_fs_on_mount,
    .open_inode = ttyfs_fs_open_inode,
    .close_inode = ttyfs_fs_close_inode,
};

static struct InodeOps s_ttyfs_inode_ops {
    .stat = ttyfs_inode_stat,
};

static struct InodeFileOps s_ttyfs_inode_file_ops {
    .read = ttyfs_file_inode_read,
    .write = ttyfs_file_inode_write,
    .ioctl = ttyfs_file_inode_ioctl,
    .seek = ttyfs_file_inode_seek,
    .poll = ttyfs_file_inode_poll,
    .mmap = ttyfs_file_inode_mmap,
};

static struct InodeDirOps s_ttyfs_inode_dir_ops {
    .lookup = ttyfs_dir_inode_lookup,
    .create = ttyfs_dir_inode_create,
    .mkdir = ttyfs_dir_inode_mkdir,
    .rmdir = ttyfs_dir_inode_rmdir,
    .unlink = ttyfs_dir_inode_unlink,
};


static int ttyfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode)
{
    int rc = 0;
    FileDevice *device = nullptr;
    LOGI("Looking up %s in inode %" PRIu64, name, self->identifier);

    LOGI("Found device %s", name);

    *out_inode = (Inode) {
        .refcount = 1,
        .type = device_type_to_inode_type(device->device_type()),
        .identifier = device->dev_id(),
        .filesystem = self->filesystem,
        .mode = 0666,
        .uid = 0,
        .size = get_device_size(device),
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .opaque = nullptr,
        .ops = &s_ttyfs_inode_ops,
        .file_ops = &s_ttyfs_inode_file_ops,
    };

    return 0;

failed:
    return rc;
}

static int ttyfs_dir_inode_create(Inode*, const char*, InodeType, Inode**)
{
    return -ERR_NOTSUP;
}

static int ttyfs_dir_inode_mkdir(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int ttyfs_dir_inode_rmdir(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int ttyfs_dir_inode_unlink(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int64_t ttyfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx*) self->opaque;
    return ctx->device->read(offset, buffer, size);
}

static int64_t ttyfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx*) self->opaque;
    return ctx->device->write(offset, buffer, size);
}

static int32_t ttyfs_file_inode_ioctl(Inode *self, uint32_t ioctl, void *argp)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx*) self->opaque;
    return ctx->device->ioctl(ioctl, argp);
}

static uint64_t ttyfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx *) self->opaque;
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

static int32_t ttyfs_file_inode_poll(Inode *self, uint32_t events, uint32_t *out_revents)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx*) self->opaque;
    return ctx->device->poll(events, out_revents);
}

static int32_t ttyfs_file_inode_mmap(Inode *self, AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t flags)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx*) self->opaque;
    return ctx->device->mmap(as, vaddr, length, flags);
}

static int ttyfs_fs_on_mount(Filesystem *self, Inode *out_root)
{
    *out_root = (Inode) {
        .refcount = 0,
        .type = InodeType::Directory,
        .identifier = 0,
        .filesystem = self,
        .mode = 0,
        .uid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .opaque = nullptr,
        .ops = &s_ttyfs_inode_ops,
        .dir_ops = &s_ttyfs_inode_dir_ops,
    };

    return 0;
}

static int ttyfs_fs_open_inode(Filesystem*, Inode *inode)
{
    int rc;
    TTYFSInodeCtx *ctx = nullptr;
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
    ctx = static_cast<TTYFSInodeCtx*>(malloc(sizeof(TTYFSInodeCtx)));
    if (ctx == nullptr) {
        LOGE("Could not allocate inode context");
        rc = -ERR_NOMEM;
        goto failed;
    }
    *ctx = TTYFSInodeCtx {
        .type = device->device_type() == Device::Type::CharacterDevice ? MountableDeviceType::CharacterDevice : MountableDeviceType::BlockDevice,
        .device = device,
    };
    inode->opaque = ctx;

    return 0;
failed:
    return rc;
}

static int ttyfs_inode_stat(Inode *self, api::Stat *st)
{
    TTYFSInodeCtx *ctx = (TTYFSInodeCtx*) self->opaque;

    st->st_dev = ctx->device->dev_id();
    st->st_ino = self->identifier;
    switch (ctx->type) {
    case MountableDeviceType::CharacterDevice: {
        st->st_mode = SF_IFCHR;
        st->st_blksize = 0;
        st->st_blocks = 0;
        break;
    }
    case MountableDeviceType::BlockDevice: {
        BlockDevice *device = reinterpret_cast<BlockDevice *>(ctx->device);
        st->st_mode = SF_IFBLK;
        st->st_blksize = device->block_size();
        st->st_blocks = round_down(device->size(), device->block_size());
        break;
    }
    }
    st->st_nlink = 1;
    st->st_uid = self->uid;
    st->st_gid = self->uid;
    st->st_rdev = 0;
    st->st_size = self->size;
    st->atim = self->access_time;
    st->mtim = self->modification_time;
    st->ctim = self->creation_time;
    
    return 0;
}

static int ttyfs_fs_close_inode(Filesystem*, Inode *inode)
{
    // TODO: Call 'flush' on the device
    free(inode->opaque);
    inode->opaque = nullptr;
    return 0;
}

int ttyfs_create(Filesystem **out_fs)
{
    int rc = 0;
    uint8_t *mem;
    TTYFSFilesystemCtx *ctx = nullptr;
    
    mem = (uint8_t*) malloc(sizeof(Filesystem) + sizeof(TTYFSFilesystemCtx));
    if (!mem) {
        rc = -ERR_NOMEM;
        goto failed;
    }

    *out_fs = (Filesystem*) mem;
    ctx = (TTYFSFilesystemCtx*) &mem[sizeof(Filesystem)];

    *ctx = TTYFSFilesystemCtx {};
    **out_fs = (Filesystem) {
        .ops = &s_ttyfs_ops,
        .icache = {},
        .root = 0,
        .opaque = ctx,
    };

    LOGI("Created ttyfs");

    return 0;
failed:
    return rc;
}
