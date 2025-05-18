#include <kernel/base.h>
#include <kernel/drivers/devicemanager.h>
#include "ptyfs.h"

#define LOG_ENABLED
#define LOG_TAG "PTYFS"
#include <kernel/log.h>


struct PtyfsFilesystemCtx {
#define MAX_OPEN_PTYS 16
#define ROOT_INODE_ID (2 * MAX_OPEN_PTYS)
    PtyMaster *masters[MAX_OPEN_PTYS];
    PtySlave *slaves[MAX_OPEN_PTYS];
};

static int ptyfs_fs_on_mount(Filesystem *self, Inode *out_root);
static int ptyfs_fs_open_inode(Filesystem*, Inode*);
static int ptyfs_fs_close_inode(Filesystem*, Inode*);

static int64_t ptyfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
static int64_t ptyfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
static int32_t ptyfs_file_inode_ioctl(Inode *self, uint32_t request, void *argp);
static uint64_t ptyfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset);
static int32_t ptyfs_file_inode_poll(Inode *self, uint32_t events, uint32_t *out_revents);
static int32_t ptyfs_file_inode_mmap(Inode*, AddressSpace*, uintptr_t, uint32_t, uint32_t);
static int32_t ptyfs_file_inode_istty(Inode*);

static int ptyfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode);
static int ptyfs_dir_inode_create(Inode *self, const char *name, InodeType type, Inode **out_inode);
static int ptyfs_dir_inode_unlink(Inode *self, const char *name);


static struct FilesystemOps s_ptyfs_ops {
    .on_mount = ptyfs_fs_on_mount,
    .open_inode = ptyfs_fs_open_inode,
    .close_inode = ptyfs_fs_close_inode,
};

static struct InodeOps s_ptyfs_inode_ops {
};

static struct InodeFileOps s_ptyfs_inode_file_ops {
    .read = ptyfs_file_inode_read,
    .write = ptyfs_file_inode_write,
    .ioctl = ptyfs_file_inode_ioctl,
    .seek = ptyfs_file_inode_seek,
    .poll = ptyfs_file_inode_poll,
    .mmap = ptyfs_file_inode_mmap,
    .istty = ptyfs_file_inode_istty,
};

static struct InodeDirOps s_ptyfs_inode_dir_ops {
    .lookup = ptyfs_dir_inode_lookup,
    .create = ptyfs_dir_inode_create,
    .unlink = ptyfs_dir_inode_unlink,
};

static constexpr uint64_t get_pty_master_inode_id(int pty_id) { return pty_id; }
static constexpr uint64_t get_pty_slave_inode_id(int pty_id) { return pty_id + MAX_OPEN_PTYS; }
static CharacterDevice *get_device_by_inode(Inode *inode)
{
    kassert(inode->identifier < 2*MAX_OPEN_PTYS);
    PtyfsFilesystemCtx *ctx = (PtyfsFilesystemCtx *) inode->filesystem->opaque;

    if (inode->identifier < MAX_OPEN_PTYS) {
        return ctx->masters[inode->identifier];
    } else {
        return ctx->slaves[inode->identifier - MAX_OPEN_PTYS];
    }
}

static int create_pty_pair(PtyfsFilesystemCtx *ctx, int pty_id)
{
    int rc = 0;
    auto *master = (PtyMaster*) malloc(sizeof(PtyMaster));
    if (master == nullptr) {
        rc = -ERR_NOMEM;
        goto failed;
    }
    new (master) PtyMaster((uint8_t) pty_id);

    LOGD("Created PTY pair %d", pty_id);
    ctx->masters[pty_id] = master;
    ctx->slaves[pty_id] = master->slave();

failed:
    return rc;
}

static int ptyfs_dir_inode_lookup_ptmx(Inode *self, Inode *out_inode)
{
    int rc = 0;
    PtyfsFilesystemCtx *fs_ctx = (PtyfsFilesystemCtx *) self->filesystem->opaque;
    int pty_id = -1;

    for (int i = 0; i < MAX_OPEN_PTYS; i++) {
        if (fs_ctx->masters[i] == nullptr && fs_ctx->slaves[i] == nullptr) {
            pty_id = i;
            break;
        }
    }
    if (pty_id == -1) {
        LOGE("Failed to allocate PTY (too many open PTYs)");
        rc = -ERR_NOENT;
        goto failed;
    }

    LOGD("Lookup for ptmx, next available PTY ID is %d", pty_id);
    *out_inode = (Inode) {
        .refcount = 1,
        .type = InodeType::CharacterDevice,
        .identifier = get_pty_master_inode_id(pty_id),
        .filesystem = self->filesystem,
        .devmajor = 0,
        .devminor = 0,
        .mode = 0666,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .blksize = 1,
        .opaque = nullptr,
        .ops = &s_ptyfs_inode_ops,
        .file_ops = &s_ptyfs_inode_file_ops,
    };

    return 0;
failed:
    return rc;
}

static int ptyfs_dir_inode_lookup_pty(Inode *self, int pty, Inode *out_inode)
{
    PtyfsFilesystemCtx *fs_ctx = (PtyfsFilesystemCtx *) self->filesystem->opaque;

    if (pty < 0 || pty >= MAX_OPEN_PTYS || fs_ctx->slaves[pty] == nullptr)
        return -ERR_NOENT;
    
    *out_inode = (Inode) {
        .refcount = 1,
        .type = InodeType::CharacterDevice,
        .identifier = get_pty_slave_inode_id(pty),
        .filesystem = self->filesystem,
        .devmajor = 0,
        .devminor = 0,
        .mode = 0666,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .blksize = 1,
        .opaque = nullptr,
        .ops = &s_ptyfs_inode_ops,
        .file_ops = &s_ptyfs_inode_file_ops,
    };

    return 0;
}

static int ptyfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode)
{
    int pty_id;

    if (strcmp("ptmx", name) == 0) {
        return ptyfs_dir_inode_lookup_ptmx(self, out_inode);
    } else if (sscanf(name, "pty%u", &pty_id) == 1) {
        return ptyfs_dir_inode_lookup_pty(self, pty_id, out_inode);
    }

    LOGD("Odd lookup for %s in ptyfs", name);

    return -ERR_NOENT;
}

static int ptyfs_dir_inode_create(Inode*, const char*, InodeType, Inode**)
{
    return -ERR_NOTSUP;
}

static int ptyfs_dir_inode_unlink(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int64_t ptyfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size)
{
    return get_device_by_inode(self)->read(offset, buffer, size);
}

static int64_t ptyfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size)
{
    return get_device_by_inode(self)->write(offset, buffer, size);
}

static int32_t ptyfs_file_inode_ioctl(Inode *self, uint32_t ioctl, void *argp)
{
    return get_device_by_inode(self)->ioctl(ioctl, argp);
}

static uint64_t ptyfs_file_inode_seek(Inode*, uint64_t, int, int32_t)
{
    return 0;
}

static int32_t ptyfs_file_inode_poll(Inode *self, uint32_t events, uint32_t *out_revents)
{
    return get_device_by_inode(self)->poll(events, out_revents);
}

static int32_t ptyfs_file_inode_mmap(Inode *self, AddressSpace *as, uintptr_t vaddr, uint32_t length, uint32_t flags)
{
    return get_device_by_inode(self)->mmap(as, vaddr, length, flags);
}

static int32_t ptyfs_file_inode_istty(Inode *self)
{
    return self->identifier == ROOT_INODE_ID ? 0 : 1;
}

static int ptyfs_fs_on_mount(Filesystem *self, Inode *out_root)
{
    *out_root = (Inode) {
        .refcount = 0,
        .type = InodeType::Directory,
        .identifier = ROOT_INODE_ID,
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
        .blksize = 1,
        .opaque = nullptr,
        .ops = &s_ptyfs_inode_ops,
        .dir_ops = &s_ptyfs_inode_dir_ops,
    };

    return 0;
}

static int ptyfs_fs_open_inode(Filesystem *fs, Inode *inode)
{
    int rc;
    if (inode->identifier == ROOT_INODE_ID)
        return 0;
    
    kassert(inode->identifier < 2*MAX_OPEN_PTYS);
    PtyfsFilesystemCtx *ctx = (PtyfsFilesystemCtx *) fs->opaque;

    if (inode->identifier < MAX_OPEN_PTYS) {
        rc = create_pty_pair(ctx, inode->identifier);
    } else {
        PtySlave *slave = ctx->slaves[inode->identifier - MAX_OPEN_PTYS];
        if (slave == nullptr) {
            LOGW("Failed to open PTY slave (this is a bug)");
            rc = -ERR_NOENT;
            goto failed;
        }
        slave->set_open(true);
    }

    return 0;
failed:
    return rc;
}

static int ptyfs_fs_close_inode(Filesystem *fs, Inode *inode)
{
    if (inode->identifier == ROOT_INODE_ID)
        return 0;

    PtyfsFilesystemCtx *ctx = (PtyfsFilesystemCtx *) fs->opaque;
    unsigned pty_id;

    kassert(inode->identifier < 2*MAX_OPEN_PTYS);
    if (inode->identifier < MAX_OPEN_PTYS) {
        pty_id = inode->identifier;

        PtyMaster *master = ctx->masters[pty_id];
        if (master == nullptr) {
            LOGW("Failed to close PTY master (this is a bug)");
            return -ERR_NOENT;
        }
        master->mark_closed();
    } else {
        pty_id = inode->identifier - MAX_OPEN_PTYS;
        PtySlave *slave = ctx->slaves[pty_id];
        if (slave == nullptr) {
            LOGW("Failed to close PTY slave (this is a bug)");
            return -ERR_NOENT;
        }
        slave->set_open(false);
    }

    if (!ctx->masters[pty_id]->is_open() && !ctx->slaves[pty_id]->is_open()) {
        free(ctx->masters[pty_id]);
        // Only free the master because the slave is inside the master
        ctx->masters[pty_id] = nullptr;
        ctx->slaves[pty_id] = nullptr;
    }

    return 0;
}

int ptyfs_create(Filesystem **out_fs)
{
    int rc = 0;
    uint8_t *mem;
    PtyfsFilesystemCtx *ctx = nullptr;
    
    mem = (uint8_t*) malloc(sizeof(Filesystem) + sizeof(PtyfsFilesystemCtx));
    if (!mem) {
        rc = -ERR_NOMEM;
        goto failed;
    }

    *out_fs = (Filesystem*) mem;
    ctx = (PtyfsFilesystemCtx*) &mem[sizeof(Filesystem)];

    *ctx = PtyfsFilesystemCtx {
        .masters = {},
        .slaves = {},
    };
    for (size_t i = 0; i < MAX_OPEN_PTYS; i++) {
        ctx->masters[i] = nullptr;
        ctx->slaves[i] = nullptr;
    }

    **out_fs = (Filesystem) {
        .ops = &s_ptyfs_ops,
        .icache = {},
        .root = ROOT_INODE_ID,
        .opaque = ctx,
    };

    LOGI("Created ptyfs");

    return 0;
failed:
    return rc;
}
