#include "pipefs.h"

#define LOG_ENABLED
#define LOG_TAG "PIPEFS"
#include <kernel/log.h>

struct PipeFSFilesystemCtx {
    uint64_t next_pipe_id;
};

struct PipeFSInodeCtx {
    uint8_t* buffer;
    size_t read_pos;
    size_t write_pos;
    size_t capacity;
};

static int pipefs_fs_on_mount(Filesystem *self, Inode *out_root);
static int pipefs_fs_open_inode(Filesystem*, Inode*);
static int pipefs_fs_close_inode(Filesystem*, Inode*);

static int pipefs_inode_stat(Inode *self, api::Stat *st);
static int64_t pipefs_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
static int64_t pipefs_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
static int pipefs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode);

static struct FilesystemOps s_pipefs_ops {
    .on_mount = pipefs_fs_on_mount,
    .open_inode = pipefs_fs_open_inode,
    .close_inode = pipefs_fs_close_inode,
};

static struct InodeOps s_pipefs_inode_ops {
    .stat = pipefs_inode_stat,
};

static struct InodeFileOps s_pipefs_inode_file_ops {
    .read = pipefs_inode_read,
    .write = pipefs_inode_write,
    .ioctl = fs_inode_ioctl_not_supported,
    .seek = fs_inode_seek_not_supported,
};

static struct InodeDirOps s_pipefs_inode_dir_ops {
    .lookup = pipefs_dir_inode_lookup,
    .create = fs_dir_inode_create_not_supported,
    .mkdir = fs_dir_inode_mkdir_not_supported,
    .rmdir = fs_dir_inode_rmdir_not_supported,
    .unlink = fs_dir_inode_unlink_not_supported,
};

static int pipefs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode)
{
    if (self->identifier != 0 || strcmp(name, "new") != 0)
        return -ERR_NOENT;

    PipeFSFilesystemCtx *fs = (PipeFSFilesystemCtx*) self->filesystem->opaque;

    LOGI("Creating new pipe inode (id: %" PRIu64 ")", fs->next_pipe_id);
    *out_inode = (Inode) {
        .refcount = 1,
        .type = InodeType::Pipe,
        .identifier = fs->next_pipe_id++,
        .filesystem = self->filesystem,
        .mode = 0666,
        .uid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .opaque = nullptr,
        .ops = &s_pipefs_inode_ops,
        .file_ops = &s_pipefs_inode_file_ops,
    };

    return 0;
}

static int pipefs_fs_on_mount(Filesystem *self, Inode *out_root)
{
    *out_root = (Inode) {
        .refcount = 0,
        .type = InodeType::Directory,
        .identifier = 0,
        .filesystem = self,
        .mode = 0755,
        .uid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .opaque = nullptr,
        .ops = &s_pipefs_inode_ops,
        .dir_ops = &s_pipefs_inode_dir_ops,
    };

    return 0;
}

static int pipefs_fs_open_inode(Filesystem*, Inode *inode)
{
    if (inode->identifier == 0)
        return 0;

    auto *ctx = static_cast<PipeFSInodeCtx*>(malloc(sizeof(PipeFSInodeCtx)));
    if (ctx == nullptr)
        return -ERR_NOMEM;

    ctx->buffer = static_cast<uint8_t*>(malloc(4096));
    if (ctx->buffer == nullptr) {
        free(ctx);
        return -ERR_NOMEM;
    }

    LOGI("Created pipe inode (id: %" PRIu64 ")", inode->identifier);
    *ctx = PipeFSInodeCtx {
        .buffer = ctx->buffer,
        .read_pos = 0,
        .write_pos = 0,
        .capacity = 4096,
    };
    
    inode->opaque = ctx;
    return 0;
}

static int64_t pipefs_inode_read(Inode *self, int64_t, uint8_t *buffer, size_t size)
{
    auto *ctx = static_cast<PipeFSInodeCtx*>(self->opaque);
    LOGI("Reading %" PRIu32 " bytes from pipe (id: %" PRIu64 ")", size, self->identifier);
    if (ctx->read_pos == ctx->write_pos)
        return 0;

    size_t bytes_read = 0;
    while (bytes_read < size && ctx->read_pos != ctx->write_pos) {
        buffer[bytes_read++] = ctx->buffer[ctx->read_pos];
        ctx->read_pos = (ctx->read_pos + 1) % ctx->capacity;
    }

    return bytes_read;
}

static int64_t pipefs_inode_write(Inode *self, int64_t, const uint8_t *buffer, size_t size)
{
    auto *ctx = static_cast<PipeFSInodeCtx*>(self->opaque);

    LOGI("Writing %" PRIu32 " bytes to pipe (id: %" PRIu64 ")", size, self->identifier);

    size_t bytes_written = 0;
    while (bytes_written < size) {
        if ((ctx->write_pos + 1) % ctx->capacity == ctx->read_pos)
            break;
            
        ctx->buffer[ctx->write_pos] = buffer[bytes_written++];
        ctx->write_pos = (ctx->write_pos + 1) % ctx->capacity;
    }

    return bytes_written;
}

static int pipefs_fs_close_inode(Filesystem*, Inode *inode)
{
    if (inode->identifier == 0)
        return 0;

    auto *ctx = static_cast<PipeFSInodeCtx*>(inode->opaque);
    free(ctx->buffer);
    free(ctx);
    inode->opaque = nullptr;
    return 0;
}

static int pipefs_inode_stat(Inode *self, api::Stat *st)
{
    st->st_dev = 0;
    st->st_ino = self->identifier;
    st->st_mode = SF_IFDIR | 0666;
    st->st_nlink = 1;
    st->st_uid = self->uid;
    st->st_gid = self->uid;
    st->st_rdev = 0;
    st->st_size = self->size;
    st->st_blksize = 4096;
    st->st_blocks = 1;
    st->atim = self->access_time;
    st->mtim = self->modification_time;
    st->ctim = self->creation_time;

    if (self->identifier == 0) {
        st->st_mode |= SF_IFDIR;
    } else {
        st->st_mode |= SF_IFIFO;
    }
    
    return 0;
}

int pipefs_create(Filesystem **out_fs)
{
    uint8_t *mem = static_cast<uint8_t*>(malloc(sizeof(Filesystem) + sizeof(PipeFSFilesystemCtx)));
    if (!mem)
        return -ERR_NOMEM;

    *out_fs = reinterpret_cast<Filesystem*>(mem);
    auto *ctx = reinterpret_cast<PipeFSFilesystemCtx*>(mem + sizeof(Filesystem));

    *ctx = PipeFSFilesystemCtx{
        .next_pipe_id = 1,
    };
    **out_fs = (Filesystem) {
        .ops = &s_pipefs_ops,
        .icache = {},
        .root = 0,
        .opaque = ctx,
    };

    LOGI("Created pipefs");
    return 0;
}
