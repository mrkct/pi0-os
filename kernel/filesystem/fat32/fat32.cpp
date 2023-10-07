#include <kernel/filesystem/fat32/fat32.h>
#include <kernel/filesystem/fat32/fat32_structures.h>
#include <kernel/kprintf.h>
#include <kernel/lib/math.h>
#include <kernel/lib/memory.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>

namespace kernel {

static constexpr size_t SECTOR_SIZE = 512;

#define READ_SECTOR(fs, idx, buf) (fs).storage->read_block(*(fs).storage, (idx), (buf))

struct Fat32RuntimeInfo {
    fat32::BiosParameterBlock bpb;
};

struct Fat32Directory {
    size_t first_cluster;
    size_t current_cluster;
    size_t next_entry_index;
};
static_assert(sizeof(Fat32Directory) <= sizeof(Directory::impl_data), "Fat32Directory is too big to fit in Directory::impl_data");

struct Fat32DirectoryEntryData {
    size_t first_cluster;
    size_t size;
};
static_assert(sizeof(Fat32DirectoryEntryData) <= sizeof(DirectoryEntry::impl_data), "Fat32DirectoryEntryData is too big to fit in DirectoryEntry::impl_data");

struct Fat32File {
    size_t first_cluster;
};
static_assert(sizeof(Fat32File) <= sizeof(File::impl_data), "Fat32File is too big to fit in File::impl_data");

static void copy_entry_name(fat32::DirectoryEntry8_3& e, char* buf)
{
    size_t name_length = 0;
    while (e.DIR_Name[name_length] != ' ' && name_length < 8)
        name_length++;

    size_t extension_length = 0;
    while (e.DIR_Name[8 + extension_length] != ' ' && extension_length < 3)
        extension_length++;

    klib::kmemcpy(buf, e.DIR_Name, name_length);
    if (extension_length > 0) {
        buf[name_length] = '.';
        klib::kmemcpy(buf + name_length + 1, e.DIR_Name + 8, extension_length);
    }
    buf[name_length + extension_length + (extension_length > 0 ? 1 : 0)] = '\0';
}

static Error next_cluster(Filesystem& fs, size_t cluster, size_t& next_cluster)
{
    auto& bpb = static_cast<Fat32RuntimeInfo*>(fs.impl_data)->bpb;
    auto fat_offset = cluster * 4;
    auto fat_sector = bpb.BPB_RsvdSecCnt + (fat_offset / SECTOR_SIZE);
    auto fat_sector_offset = fat_offset % SECTOR_SIZE;

    uint32_t table_sector[SECTOR_SIZE / sizeof(uint32_t)];
    TRY(READ_SECTOR(fs, fat_sector, reinterpret_cast<uint8_t*>(&table_sector)));

    next_cluster = table_sector[fat_sector_offset / sizeof(uint32_t)] & 0x0fffffff;

    return Success;
}

static Error step_cluster_chain(Filesystem& fs, size_t first_cluster, size_t steps, size_t& cluster)
{
    size_t current = first_cluster;
    for (size_t i = 0; i < steps; i++) {
        TRY(next_cluster(fs, current, current));
        if (cluster >= 0x0ffffff8)
            break;
    }

    cluster = current;
    return Success;
}

static size_t cluster_idx_to_sector(fat32::BiosParameterBlock& bpb, size_t cluster_idx)
{
    return bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 + (cluster_idx - 2) * bpb.BPB_SecPerClus;
}

static Error init(Filesystem& fs)
{
    static_assert(sizeof(fat32::BiosParameterBlock) == SECTOR_SIZE, "BPB size must be equal to sector size");

    Fat32RuntimeInfo info;

    TRY(READ_SECTOR(fs, 0, reinterpret_cast<uint8_t*>(&info.bpb)));

    if (info.bpb.BS_BootSig32 != fat32::BOOT_SIGNATURE) {
        return Error {
            .generic_error_code = GenericErrorCode::InvalidFormat,
            .device_specific_error_code = 0,
            .user_message = "Invalid boot signature",
            .extra_data = nullptr
        };
    }

    if (info.bpb.BPB_BytsPerSec != SECTOR_SIZE) {
        return Error {
            .generic_error_code = GenericErrorCode::NotSupported,
            .device_specific_error_code = 0,
            .user_message = "Unsupported sector size from BPB",
            .extra_data = nullptr
        };
    }

    fat32::FSInfo fs_info;
    static_assert(sizeof(fs_info) == SECTOR_SIZE, "FSInfo size must be equal to sector size");
    TRY(READ_SECTOR(fs, info.bpb.BPB_FSInfo, reinterpret_cast<uint8_t*>(&fs_info)));
    if (fs_info.FSI_LeadSig != fat32::FSINFO_LEAD_SIGNATURE || fs_info.FSI_StrucSig != fat32::FSINFO_STRUC_SIGNATURE || fs_info.FSI_TrailSig != fat32::FSINFO_TRAIL_SIGNATURE) {
        return Error {
            .generic_error_code = GenericErrorCode::InvalidFormat,
            .device_specific_error_code = 0,
            .user_message = "Invalid FSInfo signature(s)",
            .extra_data = nullptr
        };
    }

    TRY(kmalloc(sizeof(Fat32RuntimeInfo), fs.impl_data));
    *static_cast<Fat32RuntimeInfo*>(fs.impl_data) = info;

    return Success;
}

static Error root_directory(Filesystem& fs, Directory& directory)
{
    auto& fat32_directory = *reinterpret_cast<Fat32Directory*>(directory.impl_data);

    directory.fs = &fs;
    fat32_directory.first_cluster = 2;
    fat32_directory.current_cluster = fat32_directory.first_cluster;
    fat32_directory.next_entry_index = 0;

    return Success;
}

static Error directory_next_entry(Directory& dir, DirectoryEntry& entry)
{
restart:
    auto& fat_fs = *reinterpret_cast<Fat32RuntimeInfo*>(dir.fs->impl_data);
    auto& fat_dir = *reinterpret_cast<Fat32Directory*>(dir.impl_data);

    const size_t DIR_ENTRIES_IN_SECTOR = SECTOR_SIZE / sizeof(fat32::DirectoryEntry);
    const size_t DIR_ENTRIES_IN_CLUSTER = fat_fs.bpb.BPB_SecPerClus * DIR_ENTRIES_IN_SECTOR;

    auto sector_index_in_cluster = (fat_dir.next_entry_index % DIR_ENTRIES_IN_CLUSTER) / DIR_ENTRIES_IN_SECTOR;
    auto entry_offset_in_sector = fat_dir.next_entry_index % DIR_ENTRIES_IN_SECTOR;
    auto absolute_sector_index = cluster_idx_to_sector(fat_fs.bpb, fat_dir.current_cluster) + sector_index_in_cluster;

    // NOTE: We increment the entry index here, instead of at the end of the function,
    //       because we might want to retry this function if we encounter a long name entry
    //       or a deleted entry.
    fat_dir.next_entry_index++;
    if (fat_dir.next_entry_index % DIR_ENTRIES_IN_CLUSTER == 0) {
        TRY(next_cluster(*dir.fs, fat_dir.current_cluster, fat_dir.current_cluster));
    }

    fat32::DirectoryEntry sector_entries[DIR_ENTRIES_IN_SECTOR];
    TRY(READ_SECTOR(*dir.fs, absolute_sector_index, reinterpret_cast<uint8_t*>(sector_entries)));

    auto& fat_entry = sector_entries[entry_offset_in_sector];
    if (fat_entry.entry.DIR_Name[0] == 0)
        return EndOfData;

    if (fat_entry.entry.DIR_Name[0] == 0xe5)
        goto restart;

    if (fat_entry.entry.DIR_Attr == fat32::DirectoryEntry8_3::ATTR_LONG_NAME) {
        // TODO: Long name support
        goto restart;
    }

    entry.dir = &dir;
    copy_entry_name(fat_entry.entry, entry.name);
    entry.type = fat_entry.entry.DIR_Attr == fat32::DirectoryEntry8_3::ATTR_DIRECTORY ? DirectoryEntry::Type::Directory : DirectoryEntry::Type::File;
    entry.size = fat_entry.entry.DIR_FileSize;
    auto& fat_entry_data = *reinterpret_cast<Fat32DirectoryEntryData*>(entry.impl_data);
    fat_entry_data = Fat32DirectoryEntryData {
        .first_cluster = (static_cast<uint32_t>(fat_entry.entry.DIR_FstClusHI) << 16 | fat_entry.entry.DIR_FstClusLO) & 0x0fffffff,
        .size = fat_entry.entry.DIR_FileSize
    };

    return Success;
}

static Error open_file_entry(DirectoryEntry& entry, File& file)
{
    if (entry.type != DirectoryEntry::Type::File) {
        return Error {
            .generic_error_code = GenericErrorCode::NotSupported,
            .device_specific_error_code = 0,
            .user_message = "Entry is not a file",
            .extra_data = nullptr
        };
    }

    auto& fat_entry = *reinterpret_cast<Fat32DirectoryEntryData*>(entry.impl_data);
    auto& fat_file = *reinterpret_cast<Fat32File*>(file.impl_data);

    file.fs = entry.dir->fs;
    file.current_offset = 0;
    file.size = fat_entry.size;
    klib::strncpy_safe(file.name, entry.name, sizeof(file.name));
    fat_file.first_cluster = fat_entry.first_cluster;

    return Success;
}

static Error open_directory_entry(DirectoryEntry& entry, Directory& dir)
{
    if (entry.type != DirectoryEntry::Type::Directory) {
        return Error {
            .generic_error_code = GenericErrorCode::NotSupported,
            .device_specific_error_code = 0,
            .user_message = "Entry is not a directory",
            .extra_data = nullptr
        };
    }

    auto& fat_entry = *reinterpret_cast<Fat32DirectoryEntryData*>(entry.impl_data);
    auto& fat_dir = *reinterpret_cast<Fat32Directory*>(dir.impl_data);

    dir.fs = entry.dir->fs;
    fat_dir = Fat32Directory {
        .first_cluster = fat_entry.first_cluster,
        .current_cluster = fat_entry.first_cluster,
        .next_entry_index = 0
    };

    return Success;
}

static Error read(File& file, uint8_t* buffer, size_t offset, size_t size, size_t& bytes_read)
{
    auto& fat_fs = *reinterpret_cast<Fat32RuntimeInfo*>(file.fs->impl_data);
    auto& fat_file = *reinterpret_cast<Fat32File*>(file.impl_data);

    offset = klib::min<uint64_t>(offset, file.size);
    size = klib::min<uint64_t>(size, file.size - offset);

    size_t current_cluster;
    TRY(step_cluster_chain(*file.fs, fat_file.first_cluster, offset / (fat_fs.bpb.BPB_SecPerClus * SECTOR_SIZE), current_cluster));

    auto remaining_size = size;
    auto current_offset = offset;
    while (remaining_size > 0) {
        auto cluster_offset = current_offset % (fat_fs.bpb.BPB_SecPerClus * SECTOR_SIZE);
        auto remaining_size_in_cluster = klib::min(remaining_size, fat_fs.bpb.BPB_SecPerClus * SECTOR_SIZE - cluster_offset);

        while (remaining_size_in_cluster > 0) {
            auto sector_offset = current_offset % SECTOR_SIZE;
            auto to_read_in_sector = klib::min(remaining_size_in_cluster, SECTOR_SIZE - sector_offset);

            auto sector_index = cluster_idx_to_sector(fat_fs.bpb, current_cluster) + cluster_offset / SECTOR_SIZE;

            if (to_read_in_sector == SECTOR_SIZE) {
                kassert(sector_offset == 0);
                TRY(READ_SECTOR(*file.fs, sector_index, buffer));
            } else {
                kassert(sector_offset + to_read_in_sector <= SECTOR_SIZE);
                uint8_t sector_buffer[SECTOR_SIZE];
                TRY(READ_SECTOR(*file.fs, sector_index, sector_buffer));
                klib::kmemcpy(buffer, sector_buffer + sector_offset, to_read_in_sector);
            }

            cluster_offset += to_read_in_sector;
            remaining_size -= to_read_in_sector;
            remaining_size_in_cluster -= to_read_in_sector;
            current_offset += to_read_in_sector;
            buffer += to_read_in_sector;
        }

        if (remaining_size > 0) {
            TRY(next_cluster(*file.fs, current_cluster, current_cluster));
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

Error fat32_create(Filesystem& fs, Storage& storage)
{
    fs = {
        .init = init,
        .root_directory = root_directory,
        .directory_next_entry = directory_next_entry,
        .open_file_entry = open_file_entry,
        .open_directory_entry = open_directory_entry,
        .read = read,
        .storage = &storage,
        .impl_data = nullptr
    };

    return Success;
}

}
