#include <sys/dirent.h>
#include "tempfs.h"

#define LOG_ENABLED
#define LOG_TAG "TEMPFS"
#include <kernel/log.h>


static constexpr uint32_t BLOCK_SIZE = 512;
static constexpr size_t MAX_NAME_LEN = 63;


struct TempInode {
    InodeType type;
    union {
        struct {
            uint32_t allocated;
            uint32_t size;
            uint8_t *data;
        } file;
        struct {
            struct {
                char name[MAX_NAME_LEN + 1];
                TempInode *inode;
            } children[16];
        } directory;
    };
};

struct TempFsFilesystemCtx {
    uint64_t capacity;
    uint64_t used;

    TempInode *root;
};

static int tempfs_fs_on_mount(Filesystem *self, Inode *out_root);
static int tempfs_fs_open_inode(Filesystem*, Inode*);
static int tempfs_fs_close_inode(Filesystem*, Inode*);

static int64_t tempfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
static int64_t tempfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
static uint64_t tempfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset);

static int tempfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode);
static int tempfs_dir_create(Inode *self, const char *name, InodeType type, Inode *out_inode);
static int tempfs_dir_unlink(Inode *self, const char *name);
static int64_t tempfs_dir_getdents(Inode *self, int64_t offset, uint8_t *buffer, size_t size);

static int tempfs_file_inode_ftruncate(Inode *self, uint64_t size);
static uint64_t tempfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset);

static struct FilesystemOps s_tempfs_ops {
    .on_mount = tempfs_fs_on_mount,
    .open_inode = tempfs_fs_open_inode,
    .close_inode = tempfs_fs_close_inode,
};

static struct InodeOps s_tempfs_inode_ops {
    .seek = tempfs_file_inode_seek,
};

static struct InodeFileOps s_tempfs_inode_file_ops {
    .read = tempfs_file_inode_read,
    .write = tempfs_file_inode_write,
    .ioctl = fs_inode_ioctl_not_supported,
    .poll = fs_file_inode_poll_always_ready,
    .mmap = fs_file_inode_mmap_not_supported,
    .istty = fs_file_inode_istty_always_false,
};

static struct InodeDirOps s_tempfs_inode_dir_ops {
    .lookup = tempfs_dir_inode_lookup,
    .create = tempfs_dir_create,
    .unlink = tempfs_dir_unlink,
    .getdents = tempfs_dir_getdents,
};

static inline TempInode* get_inode_by_id(InodeIdentifier id)
{
    return (TempInode*) id;
}

static int tempfs_file_inode_ftruncate(Inode *self, uint64_t size)
{
    auto *ctx = static_cast<TempFsFilesystemCtx*>(self->filesystem->opaque);
    auto *inode = get_inode_by_id(self->identifier);
    kassert(inode != nullptr);
    kassert(inode->type == InodeType::RegularFile);

    uint64_t required_allocated = round_up(size, BLOCK_SIZE);

    if (required_allocated > inode->file.allocated) {
        uint64_t extra_required = required_allocated - inode->file.allocated;
        if (ctx->used + extra_required > ctx->capacity) {
            LOGW("Extending file would go over max capacity");
            return -ERR_NOMEM;
        }

        void *temp = realloc(inode->file.data, required_allocated);
        if (!temp) {
            LOGW("Failed to allocate bytes for file");
            return -ERR_NOMEM;
        }

        memset((uint8_t*) temp + inode->file.allocated, 0, extra_required);

        inode->file.data = static_cast<uint8_t*>(temp);
        ctx->used += extra_required;

    } else if (required_allocated < inode->file.allocated) {
        void *temp = realloc(inode->file.data, required_allocated);
        if (!temp) {
            LOGW("Failed to allocate bytes for file");
            return -ERR_NOMEM;
        }

        inode->file.data = static_cast<uint8_t*>(temp);
        ctx->used -= inode->file.allocated - required_allocated;
    }

    inode->file.allocated = required_allocated;
    inode->file.size = size;
    self->size = size;


    return 0;
}

static void fill_inode(Filesystem *fs, TempInode *inode, Inode *out)
{
    *out = (Inode) {
        .refcount = 0,
        .type = inode->type,
        .identifier = (uintptr_t) inode,
        .filesystem = fs,
        .devmajor = 0,
        .devminor = 0,
        .mode = 0755,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .blksize = BLOCK_SIZE,
        .opaque = nullptr,
        .ops = &s_tempfs_inode_ops,
        .file_ops = nullptr,
    };
    switch (inode->type) {
    case InodeType::RegularFile:
        out->size = inode->file.size;
        out->file_ops = &s_tempfs_inode_file_ops;
        break;
    case InodeType::Directory: {
        out->size = 0;
        out->dir_ops = &s_tempfs_inode_dir_ops;
        break;
    }
    default:
        kassert(false);
    }
}

static int tempfs_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode)
{
    TempInode *inode = get_inode_by_id(self->identifier);
    kassert(inode != nullptr);
    kassert(inode->type == InodeType::Directory);

    for (size_t i = 0; i < array_size(inode->directory.children); i++) {
        auto *child = &inode->directory.children[i];
        if (child->inode == nullptr || strcmp(child->name, name) != 0)
            continue;

        fill_inode(self->filesystem, child->inode, out_inode);
        return 0;
    }

    return -ERR_NOENT;
}

static int tempfs_dir_create(Inode *self, const char *name, InodeType type, Inode *out_inode)
{
    kassert(self->type == InodeType::Directory);

    if (strlen(name) > MAX_NAME_LEN) {
        LOGW("Name too long");
        return -ERR_NAMETOOLONG;
    }
    
    TempInode *inode = get_inode_by_id(self->identifier);
    kassert(inode != nullptr);
    
    if (type != InodeType::RegularFile && type != InodeType::Directory) {
        LOGW("Unsupported inode type %d", (int) type);
        return -ERR_INVAL;
    }

    /* Find a free idx for the new file, but also check there isn't already a file with that name */
    size_t free_idx = array_size(inode->directory.children);
    for (size_t i = 0; i < array_size(inode->directory.children); i++) {
        if (inode->directory.children[i].inode == nullptr) {
            free_idx = i;
            continue;
        }

        if (strcmp(inode->directory.children[i].name, name) == 0)
            return -ERR_EXIST;
    }
    if (free_idx == array_size(inode->directory.children)) {
        LOGW("Cannot created inode, directory is full");
        return -ERR_NOSPACE;
    }

    /* Actually alloc the inode */
    TempInode *new_inode = static_cast<TempInode*>(malloc(sizeof(TempInode)));
    if (!new_inode) {
        LOGW("Failed to allocate inode");
        return -ERR_NOMEM;
    }

    switch (type) {
    case InodeType::RegularFile:
        *new_inode = (TempInode) {
            .type = InodeType::RegularFile,
            .file = {
                .allocated = 0,
                .size = 0,
                .data = nullptr,
            },
        };
        break;
    case InodeType::Directory:
        *new_inode = (TempInode) {
            .type = InodeType::Directory,
            .directory = {
                .children = {},
            },
        };
        break;
    default:
        kassert(false);
    }
    inode->directory.children[free_idx].inode = new_inode;
    strncpy(inode->directory.children[free_idx].name, name, MAX_NAME_LEN);
    inode->directory.children[free_idx].name[MAX_NAME_LEN] = '\0';
    fill_inode(self->filesystem, new_inode, out_inode);

    self->size += sizeof(struct dirent);

    return 0;
}

static int tempfs_dir_unlink(Inode *self, const char *name)
{
    auto *ctx = static_cast<TempFsFilesystemCtx*>(self->filesystem->opaque);
    TempInode *inode = get_inode_by_id(self->identifier);
    kassert(inode != nullptr);
    kassert(self->type == InodeType::Directory);

    if (ctx->root == inode) {
        LOGW("Cannot unlink from root");
        return -ERR_INVAL;
    }

    size_t i;
    for (i = 0; i < array_size(inode->directory.children); i++) {
        if (inode->directory.children[i].inode == nullptr || strcmp(inode->directory.children[i].name, name) != 0)
            continue;
        
        break;
    }

    if (i == array_size(inode->directory.children)) {
        LOGW("No such file or directory %s", name);
        return -ERR_NOENT;
    }

    TempInode *child = inode->directory.children[i].inode;

    /* If it's a directory, check it's not empty */
    if (child->type == InodeType::Directory) {
        for (size_t j = 0; j < array_size(child->directory.children); j++) {
            if (child->directory.children[j].inode != nullptr) {
                LOGW("Directory %s is not empty", name);
                return -ERR_NOTEMPTY;
            }
        }
    }

    switch (child->type) {
    case InodeType::RegularFile: {
        free(child->file.data);
        ctx->used -= child->file.allocated;
        break;
    }
    case InodeType::Directory:
    default:
        break;
    }
    free(child);

    return 0;
}

static int64_t tempfs_dir_getdents(Inode *self, int64_t offset, uint8_t *buffer, size_t size)
{
    int64_t bytes_read = 0;

    TempInode *inode = get_inode_by_id(self->identifier);
    size = round_down(size, sizeof(struct dirent));

    for (size_t i = 0; bytes_read < size && i < array_size(inode->directory.children); i++) {
        if (inode->directory.children[i].inode == nullptr) {
            continue;
        }

        /* Skip entries until we reach the offset */
        if (offset > 0) {
            offset -= sizeof(struct dirent);
            continue;
        }

        auto *child = &inode->directory.children[i];
        auto *dent = reinterpret_cast<struct dirent*>(buffer + bytes_read);
        *dent = (struct dirent) {
            .d_ino = (uintptr_t) child->inode,
            .d_type = static_cast<uint8_t>(child->inode->type == InodeType::Directory ? DT_DIR : DT_REG),
            .d_name = {},
        };
        strncpy(dent->d_name, child->name, array_size(dent->d_name) - 1);
        dent->d_name[array_size(dent->d_name) - 1] = '\0';

        bytes_read += sizeof(struct dirent);
    }

    return bytes_read;
}

static int tempfs_fs_on_mount(Filesystem *self, Inode *out_root)
{
    auto *ctx = static_cast<TempFsFilesystemCtx*>(self->opaque);

    *out_root = (Inode) {
        .refcount = 0,
        .type = InodeType::Directory,
        .identifier = (uintptr_t) ctx->root,
        .filesystem = self,
        .devmajor = 0,
        .devminor = 0,
        .mode = 0755,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .blksize = BLOCK_SIZE,
        .opaque = nullptr,
        .ops = &s_tempfs_inode_ops,
        .dir_ops = &s_tempfs_inode_dir_ops,
    };
    LOGD("Mounted tempfs, root inode is %p", ctx->root);

    return 0;
}

static int tempfs_fs_open_inode(Filesystem*, Inode*)
{
    return 0;
}

static int64_t tempfs_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size)
{
    LOGI("Reading %" PRIu32 " bytes from tempfs file (id: %" PRIu64 ")", size, self->identifier);

    TempInode *inode = get_inode_by_id(self->identifier);
    kassert(inode != nullptr);
    kassert(self->type == InodeType::RegularFile);

    if ((uint64_t) offset + size > self->size) {
        size = self->size - offset;
    }

    memcpy(buffer, inode->file.data + offset, size);

    return size;
}

static int64_t tempfs_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size)
{
    LOGI("Writing %" PRIu32 " bytes to inode (id: %" PRIu64 ")", size, self->identifier);

    int rc = 0;
    TempInode *inode = get_inode_by_id(self->identifier);
    kassert(inode != nullptr);
    kassert(self->type == InodeType::RegularFile);

    if ((uint64_t) offset + size > self->size) {
        rc = tempfs_file_inode_ftruncate(self, offset + size);
        if (rc < 0)
            return rc;
    }

    memcpy(inode->file.data + offset, buffer, size);
    return size;
}

static uint64_t tempfs_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset)
{
    return default_checked_seek(self->size, current, whence, offset);
}

static int tempfs_fs_close_inode(Filesystem*, Inode*)
{
    return 0;
}

int tempfs_create(Filesystem **out_fs, size_t capacity)
{
    uint8_t *mem = static_cast<uint8_t*>(malloc(sizeof(Filesystem) + sizeof(TempFsFilesystemCtx) + sizeof(TempInode)));
    if (!mem)
        return -ERR_NOMEM;

    *out_fs = reinterpret_cast<Filesystem*>(mem);
    mem += sizeof(Filesystem);
    auto *ctx = reinterpret_cast<TempFsFilesystemCtx*>(mem);
    mem += sizeof(TempFsFilesystemCtx);

    *ctx = TempFsFilesystemCtx{
        .capacity = capacity,
        .used = 0,
        .root = reinterpret_cast<TempInode*>(mem),
    };
    **out_fs = (Filesystem) {
        .ops = &s_tempfs_ops,
        .icache = {},
        .root = reinterpret_cast<uintptr_t>(ctx->root),
        .opaque = ctx,
    };

    *ctx->root = (TempInode) {
        .type = InodeType::Directory,
        .directory = {
            .children = {},
        },
    };

    LOGI("Created tempfs, root inode is %p", ctx->root);
    return 0;
}
