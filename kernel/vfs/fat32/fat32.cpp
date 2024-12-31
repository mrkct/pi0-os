#include "fat32.h"
#include "fat32_structures.h"

// #define LOG_ENABLED
#define LOG_TAG "FAT32"
#include <kernel/log.h>


#undef TRY
#define TRY(expr)       \
    do {                \
        rc = (expr);    \
        if (rc != 0)    \
            return rc;  \
    } while(0)


static constexpr uint32_t SECTOR_SIZE = 512;
static constexpr uint32_t INVALID_SECTOR = 0xffffffff;

static_assert(sizeof(fat32::BiosParameterBlock) == SECTOR_SIZE,
    "BPB size must be equal to sector size");
static_assert(sizeof(fat32::FSInfo) == SECTOR_SIZE,
    "FSInfo size must be equal to sector size");

struct Fat32FilesystemCtx {
    BlockDevice *storage;
    fat32::BiosParameterBlock bpb;
    fat32::FSInfo info;
    uint64_t sector_size;

    struct {
        uint32_t sector_idx;
        uint8_t buffer[SECTOR_SIZE];
    } fatcache;
};

struct Fat32OpenInodeCtx {
    uint32_t first_cluster;
    uint32_t filesize;
};

static int fat32_fs_on_mount(Filesystem *self, Inode *out_root);
static int fat32_fs_open_inode(Filesystem*, Inode*);
static int fat32_fs_close_inode(Filesystem*, Inode*);

static int fat32_inode_stat(Inode *self, api::Stat *st);

static int64_t fat32_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size);
static int64_t fat32_file_inode_write(Inode *self, int64_t offset, const uint8_t *buffer, size_t size);
static int32_t fat32_file_inode_ioctl(Inode *self, uint32_t request, void *argp);
static uint64_t fat32_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset);

static int fat32_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode);
static int fat32_dir_inode_create(Inode *self, const char *name, InodeType type, Inode **out_inode);
static int fat32_dir_inode_mkdir(Inode *self, const char *name);
static int fat32_dir_inode_rmdir(Inode *self, const char *name);
static int fat32_dir_inode_unlink(Inode *self, const char *name);


static struct FilesystemOps s_fat32_ops {
    .on_mount = fat32_fs_on_mount,
    .open_inode = fat32_fs_open_inode,
    .close_inode = fat32_fs_close_inode,
};

static struct InodeOps s_fat32_inode_ops {
    .stat = fat32_inode_stat,
};

static struct InodeFileOps s_fat32_inode_file_ops {
    .read = fat32_file_inode_read,
    .write = fat32_file_inode_write,
    .ioctl = fat32_file_inode_ioctl,
    .seek = fat32_file_inode_seek,
};

static struct InodeDirOps s_fat32_inode_dir_ops {
    .lookup = fat32_dir_inode_lookup,
    .create = fat32_dir_inode_create,
    .mkdir = fat32_dir_inode_mkdir,
    .rmdir = fat32_dir_inode_rmdir,
    .unlink = fat32_dir_inode_unlink,
};

static inline constexpr uint32_t cluster_idx_to_sector(fat32::BiosParameterBlock &bpb, uint32_t cluster_idx)
{
    return bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 + (cluster_idx - 2) * bpb.BPB_SecPerClus;
}

static inline int read_sector(Fat32FilesystemCtx *ctx, uint64_t sector_idx, void *buffer)
{
    int64_t rc = ctx->storage->read(ctx->sector_size * sector_idx, reinterpret_cast<uint8_t*>(buffer), ctx->sector_size);
    
    if (rc == (int64_t) ctx->sector_size)
        return 0;
    else if (rc > 0)
        return -ERR_IO;

    return rc;
}

static int next_cluster(Fat32FilesystemCtx *ctx, uint32_t cluster, uint32_t& next_cluster)
{
    ssize_t rc = 0;
    auto& bpb = ctx->bpb;
    auto fat_offset = cluster * 4;
    auto fat_sector = bpb.BPB_RsvdSecCnt + (fat_offset / SECTOR_SIZE);
    auto fat_sector_offset = fat_offset % SECTOR_SIZE;

    if (ctx->fatcache.sector_idx != fat_sector) {
        // Because the underlying storage might trash the buffer if it fails to read
        ctx->fatcache.sector_idx = INVALID_SECTOR;
        rc = read_sector(ctx, fat_sector, ctx->fatcache.buffer);
        if (rc < 0) {
            return (int) rc;
        }

        ctx->fatcache.sector_idx = fat_sector;
    }

    uint32_t *p = reinterpret_cast<uint32_t*>(ctx->fatcache.buffer + fat_sector_offset);
    next_cluster = (*p) & 0x0fffffff;
    return 0;
}

static int step_cluster_chain(
    Fat32FilesystemCtx *ctx,
    uint32_t first_cluster,
    uint32_t steps,
    uint32_t& out_cluster
)
{
    int rc;
    uint32_t current = first_cluster;
    for (uint32_t i = 0; i < steps; i++) {
        rc = next_cluster(ctx, current, current);
        if (rc != 0)
            return rc;
        
        if (!fat32::is_valid_cluster(current))
            break;
    }

    out_cluster = current;
    return 0;
}

static void copy_entry_name(fat32::DirectoryEntry8_3& e, char* buf)
{
    uint32_t name_length = 0;
    while (e.DIR_Name[name_length] != ' ' && name_length < 8)
        name_length++;

    uint32_t extension_length = 0;
    while (e.DIR_Name[8 + extension_length] != ' ' && extension_length < 3)
        extension_length++;

    memcpy(buf, e.DIR_Name, name_length);
    if (extension_length > 0) {
        buf[name_length] = '.';
        memcpy(buf + name_length + 1, e.DIR_Name + 8, extension_length);
    }
    buf[name_length + extension_length + (extension_length > 0 ? 1 : 0)] = '\0';
}

template<typename HandleEntry>
static int foreach_8_3_directory_entry(Inode *dirinode, HandleEntry handle_entry_cb)
{
    int rc;
    kassert(dirinode->type == InodeType::Directory);

    auto *fsctx = (Fat32FilesystemCtx*) dirinode->filesystem->opaque;

    const uint32_t DIR_ENTRIES_IN_SECTOR = fsctx->sector_size / sizeof(fat32::DirectoryEntry);

    uint32_t entry_idx = 0;
    uint32_t next_cluster_idx = dirinode->identifier;
    while (fat32::is_valid_cluster(next_cluster_idx)) {
        uint32_t cluster_start_sector = cluster_idx_to_sector(fsctx->bpb, next_cluster_idx);
        for (uint32_t sector_idx = 0; sector_idx < fsctx->bpb.BPB_SecPerClus; sector_idx++) {
            uint32_t i;
            uint8_t sector[SECTOR_SIZE];
            auto *entries = reinterpret_cast<fat32::DirectoryEntry8_3*>(sector);

            TRY(read_sector(fsctx, cluster_start_sector + sector_idx, sector));
            for (i = 0; i < DIR_ENTRIES_IN_SECTOR; i++) {
                if (entries[i].is_end_of_directory())
                    break;
                if (entries[i].is_cancelled() || entries[i].DIR_Attr == fat32::DirectoryEntry8_3::ATTR_LONG_NAME)
                    continue;

                if (handle_entry_cb(entry_idx, entries[i])) {
                    return 0;
                }

                entry_idx++;
            }

            if (i != DIR_ENTRIES_IN_SECTOR) {
                LOGD("Reached end of directory (%" PRIu32 " entries read)", i);
                break;
            }
        }

        TRY(next_cluster(fsctx, next_cluster_idx, next_cluster_idx));
    }

    return -ERR_NOENT;
}

static api::TimeSpec fat32_datetime_to_timespec(uint16_t date, uint16_t time)
{
    (void) date;
    (void) time;

    // TODO: Actually do the conversion
    return (api::TimeSpec) { .seconds = 0, .nanoseconds = 0 };
}

static int fat32_dir_inode_lookup(Inode *self, const char *name, Inode *out_inode)
{
    LOGI("Looking up %s in inode %" PRIu64, name, self->identifier);
    return foreach_8_3_directory_entry(self, [&](auto, fat32::DirectoryEntry8_3 &fat_entry) {

        char entry_name[20];
        copy_entry_name(fat_entry, entry_name);
        LOGD("- %s (%" PRIu32 " b)", entry_name, fat_entry.DIR_FileSize);
        if (0 != strcasecmp(name, entry_name))
            return false;

        InodeType inodetype = InodeType::RegularFile;
        uint32_t mode = 0;
        if (fat_entry.DIR_Attr & fat32::DirectoryEntry8_3::ATTR_DIRECTORY) {
            mode |= SF_IFDIR;
            inodetype = InodeType::Directory;
        } else {
            mode |= SF_IFREG;
            inodetype = InodeType::RegularFile;
        }
        // TODO: Figure out the mode bits

        uint32_t cluster = ((uint32_t)(fat_entry.DIR_FstClusHI) << 16 | fat_entry.DIR_FstClusLO) & 0x0fffffff;

        *out_inode = Inode {
            .refcount = 0,
            .type = inodetype,
            .identifier = cluster,
            .filesystem = self->filesystem,
            .mode = mode,
            .uid = 0,
            .size = fat_entry.DIR_FileSize,
            .access_time = fat32_datetime_to_timespec(fat_entry.DIR_LstAccDate, 0 /* no access time in fat32*/),
            .creation_time = fat32_datetime_to_timespec(fat_entry.DIR_CrtDate, fat_entry.DIR_CrtTime),
            .modification_time = fat32_datetime_to_timespec(fat_entry.DIR_WrtDate, fat_entry.DIR_WrtTime),
            .opaque = nullptr,
            .ops = &s_fat32_inode_ops,
            .file_ops = nullptr, // ignore, just to silence the compiler
        };
        if (inodetype == InodeType::RegularFile) {
            out_inode->file_ops = &s_fat32_inode_file_ops;
        } else {
            out_inode->dir_ops = &s_fat32_inode_dir_ops;
        }
        LOGI("Opened inode %" PRIu64 " for path %s, has size: %" PRIu64, out_inode->identifier, name, out_inode->size);

        return true;
    });
}

static int fat32_dir_inode_create(Inode*, const char*, InodeType, Inode**)
{
    return -ERR_NOTSUP;
}

static int fat32_dir_inode_mkdir(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int fat32_dir_inode_rmdir(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int fat32_dir_inode_unlink(Inode*, const char*)
{
    return -ERR_NOTSUP;
}

static int64_t fat32_file_inode_read(Inode *self, int64_t offset, uint8_t *buffer, size_t size)
{
    int rc;
    int64_t bytes_read = 0;
    auto *ctx = (Fat32FilesystemCtx*) self->filesystem->opaque;
    uint32_t first_cluster = self->identifier; // We use the inode's first cluster as its identifier

    offset = min<uint64_t>(offset, self->size);
    size = min<uint64_t>(size, self->size - offset);

    uint32_t current_cluster;
    TRY(step_cluster_chain(ctx,
        first_cluster,
        offset / (ctx->bpb.BPB_SecPerClus * ctx->sector_size),
        current_cluster));

    auto remaining_size = size;
    auto current_offset = offset;
    while (remaining_size > 0) {
        auto offset_in_cluster = current_offset % (ctx->bpb.BPB_SecPerClus * ctx->sector_size);
        auto remaining_size_in_cluster = min<uint64_t>(remaining_size, ctx->bpb.BPB_SecPerClus * ctx->sector_size - offset_in_cluster);

        uint64_t diskoff = ctx->sector_size * cluster_idx_to_sector(ctx->bpb, current_cluster) + offset_in_cluster;

        int64_t read = ctx->storage->read(diskoff, buffer, remaining_size_in_cluster);
        if (read < 0) {
            LOGW("Storage read returned an error: %d", (int) read);
            return (int) read;
        } else if ((uint64_t) read != remaining_size_in_cluster) {
            LOGW("Storage read returned %" PRId64 " bytes, expected %" PRIu64 " bytes", read, remaining_size_in_cluster);
            return -ERR_IO;
        }

        bytes_read += read;
        remaining_size -= read;
        current_offset += read;
        buffer += read;

        if (remaining_size > 0) {
            TRY(next_cluster(ctx, current_cluster, current_cluster));
            if (current_cluster >= 0x0ffffff8 || current_cluster < 2)
                return -ERR_IO;
        }
    }

    return bytes_read;
}

static int64_t fat32_file_inode_write(Inode *, int64_t, const uint8_t *, size_t)
{
    return -ERR_NOTSUP;
}

static int32_t fat32_file_inode_ioctl(Inode*, uint32_t, void*)
{
    return -ERR_NOTSUP;
}

static uint64_t fat32_file_inode_seek(Inode *self, uint64_t current, int whence, int32_t offset)
{
    return default_checked_seek(self->size, current, whence, offset);
}

static int fat32_fs_on_mount(Filesystem *self, Inode *out_root)
{
    *out_root = (Inode) {
        .refcount = 0,
        .type = InodeType::Directory,
        .identifier = 2,
        .filesystem = self,
        .mode = 0,
        .uid = 0,
        .size = 0,
        .access_time = {},
        .creation_time = {},
        .modification_time = {},
        .opaque = nullptr,
        .ops = &s_fat32_inode_ops,
        .dir_ops = &s_fat32_inode_dir_ops,
    };

    return 0;
}

static int fat32_fs_open_inode(Filesystem*, Inode *inode)
{
    inode->opaque = nullptr;
    return 0;
}

static int fat32_inode_stat(Inode *self, api::Stat *st)
{
    auto *fsctx = (Fat32FilesystemCtx*) self->filesystem->opaque;

    uint16_t devno = (((uint16_t) fsctx->storage->major()) << 8) | fsctx->storage->minor();

    st->st_dev = devno;
    st->st_ino = self->identifier;
    st->st_mode = self->type == InodeType::Directory ? SF_IFDIR : SF_IFREG;
    st->st_nlink = 1;
    st->st_uid = self->uid;
    st->st_gid = self->uid;
    st->st_rdev = 0;
    st->st_size = self->size;
    st->atim = self->access_time;
    st->mtim = self->modification_time;
    st->ctim = self->creation_time;
    st->st_blksize = SECTOR_SIZE;
    st->st_blocks = round_up<uint64_t>(self->size, SECTOR_SIZE) / SECTOR_SIZE;

    return 0;
}

static int fat32_fs_close_inode(Filesystem*, Inode *inode)
{
    // TODO: Write back the inode to storage, if necessary
    free(inode->opaque);
    inode->opaque = nullptr;

    return 0;
}

int fat32_try_create(BlockDevice &storage, Filesystem **out_fs)
{
    fat32::BiosParameterBlock bpb;
    fat32::FSInfo fs_info;
    ssize_t read;
    
    read = storage.read(0, (uint8_t*) &bpb, sizeof(bpb));
    if (read < 0) {
        LOGW("Call to .read for BPB failed: %d", (int) read);
        return (int) read;
    } else if (read < (ssize_t) sizeof(bpb)) {
        LOGW("Read less than expected from storage for BPB");
        return -ERR_INVAL;
    }

    if (bpb.BS_BootSig32 != fat32::BOOT_SIGNATURE) {
        LOGW("BPB signature mismatch");
        return -ERR_INVAL;
    } else if (bpb.BPB_BytsPerSec != SECTOR_SIZE) {
        LOGE("Unsupported sector size %u", (unsigned) bpb.BPB_BytsPerSec);
        return -ERR_INVAL;
    }
    
    read = storage.read(bpb.BPB_BytsPerSec * bpb.BPB_FSInfo, (uint8_t*) &fs_info, sizeof(fs_info));
    if (read < 0) {
        LOGW("Call to .read for FS_Info failed: %d", (int) read);
        return (int) read;
    } else if (read < (ssize_t) sizeof(fs_info)) {
        LOGW("Read less than expected from storage for FS_Info");
        return -ERR_INVAL;
    }

    if (fs_info.FSI_LeadSig != fat32::FSINFO_LEAD_SIGNATURE ||
        fs_info.FSI_StrucSig != fat32::FSINFO_STRUC_SIGNATURE ||
        fs_info.FSI_TrailSig != fat32::FSINFO_TRAIL_SIGNATURE) {

        LOGE("Invalid FSInfo signature(s)");
        return -ERR_INVAL;
    }

    uint8_t *mem = (uint8_t*) malloc(sizeof(Filesystem) + sizeof(Fat32FilesystemCtx));
    if (!mem)
        return -ERR_NOMEM;

    Filesystem *fs = (Filesystem*) mem;
    Fat32FilesystemCtx *ctx = (Fat32FilesystemCtx*) (mem + sizeof(Filesystem));

    *ctx = Fat32FilesystemCtx {
        .storage = &storage,
        .bpb = bpb,
        .info = fs_info,
        .sector_size = bpb.BPB_BytsPerSec,
        .fatcache = {
            .sector_idx = INVALID_SECTOR,
            .buffer = {0},
        },
    };

    *fs = Filesystem {
        .ops = &s_fat32_ops,
        .icache = {},
        .root = 2, // cluster of the root directory
        .opaque = ctx,
    };
    // FIXME: We should handle this, but the init's impl always succeeds anyway
    kassert(0 == inode_cache_init(&fs->icache));

    *out_fs = fs;
    return 0;
}
