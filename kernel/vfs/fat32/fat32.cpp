#include <kernel/lib/string.h>
#include <kernel/lib/math.h>
#include <kernel/memory/kheap.h>

#include <kernel/vfs/fat32/fat32.h>
#include <kernel/vfs/fat32/fat32_structures.h>
#include <kernel/vfs/vfs.h>
#include <api/files.h>


namespace kernel {

static constexpr uint32_t SECTOR_SIZE = 512;

#define READ_SECTOR(fs, idx, buf) (fs).storage->read_block(*(fs).storage, (idx), (buf))

struct Fat32Context {
    Storage *storage;
    fat32::BiosParameterBlock bpb;
    fat32::FSInfo info;
};

struct Fat32OpenDirectoryContext {
    uint32_t first_cluster;
    uint32_t current_cluster;
    uint32_t next_entry_index;
};

struct Fat32OpenFileContext {
    uint32_t first_cluster;
};

struct Fat32DirectoryEntryContext {
    uint32_t first_cluster;
};
static_assert(
    sizeof(Fat32DirectoryEntryContext) <= sizeof(DirectoryEntry::opaque),
    "Fat32DirectoryEntryData is too big to fit in DirectoryEntry::opaque");

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

static Error next_cluster(Fat32Context& ctx, uint32_t cluster, uint32_t& next_cluster)
{
    auto& bpb = ctx.bpb;
    auto fat_offset = cluster * 4;
    auto fat_sector = bpb.BPB_RsvdSecCnt + (fat_offset / SECTOR_SIZE);
    auto fat_sector_offset = fat_offset % SECTOR_SIZE;

    uint32_t table_sector[SECTOR_SIZE / sizeof(uint32_t)];
    TRY(READ_SECTOR(ctx, fat_sector, reinterpret_cast<uint8_t*>(&table_sector)));

    next_cluster = table_sector[fat_sector_offset / sizeof(uint32_t)] & 0x0fffffff;

    return Success;
}

static Error step_cluster_chain(Fat32Context &ctx, uint32_t first_cluster, uint32_t steps, uint32_t& out_cluster)
{
    uint32_t current = first_cluster;
    for (uint32_t i = 0; i < steps; i++) {
        TRY(next_cluster(ctx, current, current));
        if (!fat32::is_valid_cluster(current))
            break;
    }

    out_cluster = current;
    return Success;
}

static uint32_t cluster_idx_to_sector(fat32::BiosParameterBlock& bpb, uint32_t cluster_idx)
{
    return bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 + (cluster_idx - 2) * bpb.BPB_SecPerClus;
}

static Error init(Storage &storage, Fat32Context &ctx)
{
    static_assert(sizeof(fat32::BiosParameterBlock) == SECTOR_SIZE, "BPB size must be equal to sector size");

    ctx.storage = &storage;

    TRY(READ_SECTOR(ctx, 0, reinterpret_cast<uint8_t*>(&ctx.bpb)));

    if (ctx.bpb.BS_BootSig32 != fat32::BOOT_SIGNATURE) {
        return Error {
            .generic_error_code = GenericErrorCode::InvalidFormat,
            .device_specific_error_code = 0,
            .user_message = "Invalid boot signature",
            .extra_data = nullptr
        };
    }

    if (ctx.bpb.BPB_BytsPerSec != SECTOR_SIZE) {
        return Error {
            .generic_error_code = GenericErrorCode::NotSupported,
            .device_specific_error_code = 0,
            .user_message = "Unsupported sector size from BPB",
            .extra_data = nullptr
        };
    }

    fat32::FSInfo fs_info;
    static_assert(sizeof(fs_info) == SECTOR_SIZE, "FSInfo size must be equal to sector size");
    TRY(READ_SECTOR(ctx, ctx.bpb.BPB_FSInfo, reinterpret_cast<uint8_t*>(&fs_info)));
    if (fs_info.FSI_LeadSig != fat32::FSINFO_LEAD_SIGNATURE || fs_info.FSI_StrucSig != fat32::FSINFO_STRUC_SIGNATURE || fs_info.FSI_TrailSig != fat32::FSINFO_TRAIL_SIGNATURE) {
        return Error {
            .generic_error_code = GenericErrorCode::InvalidFormat,
            .device_specific_error_code = 0,
            .user_message = "Invalid FSInfo signature(s)",
            .extra_data = nullptr
        };
    }

    ctx.info = fs_info;

    return Success;
}

static Error get_root_directory(Filesystem &fs, DirectoryEntry &out_entry)
{
    out_entry.fs = &fs;
    strcpy(out_entry.name, "");
    out_entry.filetype = api::Directory;
    out_entry.size = 0;

    Fat32DirectoryEntryContext &dir_ctx = *reinterpret_cast<Fat32DirectoryEntryContext*>(out_entry.opaque);
    dir_ctx.first_cluster = 2;

    return Success;
}

template<typename HandleEntry>
static Error foreach_8_3_directory_entry(File &directory, HandleEntry handle_entry_cb)
{
    auto &ctx = *static_cast<Fat32Context*>(directory.fs->opaque);
    auto &dirctx = *static_cast<Fat32OpenDirectoryContext*>(directory.opaque);

    const uint32_t DIR_ENTRIES_IN_SECTOR = SECTOR_SIZE / sizeof(fat32::DirectoryEntry);

    uint32_t entry_idx = 0;
    uint32_t next_cluster_idx = dirctx.first_cluster;
    while (fat32::is_valid_cluster(next_cluster_idx)) {
        uint32_t cluster_start_sector = cluster_idx_to_sector(ctx.bpb, next_cluster_idx);
        for (uint32_t sector_idx = 0; sector_idx < ctx.bpb.BPB_SecPerClus; sector_idx++) {
            uint32_t i;
            uint8_t sector[SECTOR_SIZE];
            auto *entries = reinterpret_cast<fat32::DirectoryEntry8_3*>(sector);

            TRY(READ_SECTOR(ctx, cluster_start_sector + sector_idx, sector));
            for (i = 0; i < DIR_ENTRIES_IN_SECTOR; i++) {
                if (entries[i].is_end_of_directory())
                    break;
                if (entries[i].is_cancelled() || entries[i].DIR_Attr == fat32::DirectoryEntry8_3::ATTR_LONG_NAME)
                    continue;

                if (!handle_entry_cb(entry_idx, entries[i]))
                    return Success;

                entry_idx++;
            }

            if (i != DIR_ENTRIES_IN_SECTOR) {
                break;
            }
        }

        TRY(next_cluster(ctx, next_cluster_idx, next_cluster_idx));
    }

    return Success;
}

static Error directory_file_read(File &file, uint64_t offset, uint8_t *buffer, uint32_t size, uint32_t &bytes_read)
{
    size = round_down<uint32_t>(size, sizeof(DirectoryEntry));

    uint32_t entries_to_read = size / sizeof(DirectoryEntry);
    uint32_t starting_from = offset / sizeof(DirectoryEntry);
    DirectoryEntry *user_entries = (DirectoryEntry*) buffer;

    if (entries_to_read == 0) {
        bytes_read = 0;
        return Success;
    }

    // FIXME: This iterates over the entire directory every time...
    return foreach_8_3_directory_entry(file, [&](auto idx, fat32::DirectoryEntry8_3 &fat_entry) {
        if (idx < starting_from)
            return true;
        
        DirectoryEntry &this_entry = user_entries[idx - starting_from];
        auto &entry_ctx = *reinterpret_cast<Fat32DirectoryEntryContext*>(this_entry.opaque);

        this_entry.fs = file.fs;
        this_entry.size = fat_entry.DIR_FileSize;
        copy_entry_name(fat_entry, this_entry.name);
        if (fat_entry.DIR_Attr == fat32::DirectoryEntry8_3::ATTR_DIRECTORY) {
            this_entry.filetype = api::FileType::Directory;
        } else {
            this_entry.filetype = api::FileType::RegularFile;
        }
        entry_ctx = Fat32DirectoryEntryContext {
            .first_cluster = (
                static_cast<uint32_t>(fat_entry.DIR_FstClusHI) << 16 | fat_entry.DIR_FstClusLO) & 0x0fffffff
        };

        bytes_read += sizeof(DirectoryEntry);

        return !(idx == starting_from + entries_to_read - 1);
    });
}

static Error regular_file_read(File &file, uint64_t offset, uint8_t *buffer, uint32_t size, uint32_t &bytes_read)
{
    auto &ctx = *reinterpret_cast<Fat32Context*>(file.fs->opaque);
    auto &filectx = *reinterpret_cast<Fat32OpenFileContext*>(file.opaque);

    offset = min<uint64_t>(offset, file.size);
    size = min<uint64_t>(size, file.size - offset);

    uint32_t current_cluster;
    TRY(step_cluster_chain(ctx,
        filectx.first_cluster,
        offset / (ctx.bpb.BPB_SecPerClus * SECTOR_SIZE),
        current_cluster));

    auto remaining_size = size;
    auto current_offset = offset;
    while (remaining_size > 0) {
        auto cluster_offset = current_offset % (ctx.bpb.BPB_SecPerClus * SECTOR_SIZE);
        auto remaining_size_in_cluster = min<uint64_t>(remaining_size, ctx.bpb.BPB_SecPerClus * SECTOR_SIZE - cluster_offset);

        while (remaining_size_in_cluster > 0) {
            auto sector_offset = current_offset % SECTOR_SIZE;
            auto to_read_in_sector = min<uint64_t>(remaining_size_in_cluster, SECTOR_SIZE - sector_offset);

            auto sector_index = cluster_idx_to_sector(ctx.bpb, current_cluster) + cluster_offset / SECTOR_SIZE;

            if (to_read_in_sector == SECTOR_SIZE) {
                kassert(sector_offset == 0);
                TRY(READ_SECTOR(ctx, sector_index, buffer));
            } else {
                kassert(sector_offset + to_read_in_sector <= SECTOR_SIZE);
                uint8_t sector_buffer[SECTOR_SIZE];
                TRY(READ_SECTOR(ctx, sector_index, sector_buffer));
                memcpy(buffer, sector_buffer + sector_offset, to_read_in_sector);
            }

            cluster_offset += to_read_in_sector;
            remaining_size -= to_read_in_sector;
            remaining_size_in_cluster -= to_read_in_sector;
            current_offset += to_read_in_sector;
            buffer += to_read_in_sector;
        }

        if (remaining_size > 0) {
            TRY(next_cluster(ctx, current_cluster, current_cluster));
            if (current_cluster >= 0x0ffffff8) {
                return Error {
                    .generic_error_code = GenericErrorCode::InvalidFormat,
                    .device_specific_error_code = 0,
                    .user_message = "Unexpected end of file",
                    .extra_data = nullptr
                };
            }
        }
    }

    bytes_read = size;
    return Success;
}

static Error open_regular_file_entry(DirectoryEntry& entry, File &out_file)
{
    kassert(entry.filetype == api::RegularFile);

    Fat32OpenFileContext *filectx;
    TRY(kmalloc(sizeof(*filectx), filectx));

    auto &direntctx = *reinterpret_cast<Fat32DirectoryEntryContext*>(entry.opaque);

    *filectx = Fat32OpenFileContext {
        .first_cluster = direntctx.first_cluster
    };

    out_file = File {
        .fs = entry.fs,
        .refcount = 0,
        .filetype = entry.filetype,
        .size = entry.size,
        .opaque = filectx,

        .read = regular_file_read,
        .write = NULL,
        .seek = NULL,
        .close = [](auto &file) { return kfree(file.opaque); }
    };

    return Success;
}

static Error open_directory_entry(DirectoryEntry& entry, File &out_file)
{
    kassert(entry.filetype == api::Directory);

    Fat32OpenDirectoryContext *dirctx;
    TRY(kmalloc(sizeof(*dirctx), dirctx));

    auto &direntctx = *reinterpret_cast<Fat32DirectoryEntryContext*>(entry.opaque);
    
    *dirctx = {
        .first_cluster = direntctx.first_cluster,
        .current_cluster = direntctx.first_cluster,
        .next_entry_index = 0
    };

    out_file = File {
        .fs = entry.fs,
        .refcount = 0,
        .filetype = entry.filetype,
        .size = entry.size,
        .opaque = dirctx,

        .read = directory_file_read,
        .write = NULL,
        .seek = NULL,
        .close = [](auto &file) { return kfree(file.opaque); }
    };

    return Success;
}

static Error open(DirectoryEntry &entry, File &out_file)
{
    switch (entry.filetype) {
    case api::RegularFile:
        return open_regular_file_entry(entry, out_file);
    case api::Directory:
        return open_directory_entry(entry, out_file);
    default:
        return NotSupported;
    }
}

Error fat32_create(Filesystem& fs, Storage& storage)
{
    Fat32Context *ctx;
    TRY(kmalloc(sizeof(*ctx), ctx));

    if (auto err = init(storage, *ctx); !err.is_success()) {
        kfree(ctx);
        return err;
    }

    fs = {
        .is_case_sensitive = false,
        .opaque = ctx,
        .root_directory = get_root_directory,
        .open = open
    };

    return Success;
}

}
