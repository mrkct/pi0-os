#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/lib/math.h>
#include <kernel/lib/string.h>
#include <kernel/lib/memory.h>
#include <kernel/device/systimer.h>
#include <kernel/vfs/sysfs/sysfs.h>
#include <kernel/device/keyboard.h>


namespace kernel {

enum class SysfsFileId { FsRoot, MachineId, CurrentTime, Keyboard };

struct SysfsDirectoryEntryContext {
    SysfsFileId file_id;
};

struct SysfsOpenFileContext {
    SysfsFileId file_id;
};



static DirectoryEntry make_dirent(SysfsFileId file_id, const char *name, api::FileType filetype);
static Error get_root_directory(Filesystem&, DirectoryEntry &out_entry);
static Error open(DirectoryEntry &entry, File &out_file);

static Filesystem sysfs = {
    .is_case_sensitive = true,
    .opaque = NULL,
    .root_directory = get_root_directory,
    .open = open
};

static DirectoryEntry ROOT_DIRECTORY_ENTRIES[4];

static DirectoryEntry make_dirent(
    SysfsFileId file_id,
    const char *name,
    api::FileType filetype
)
{
    DirectoryEntry entry = {};
    entry.fs = &sysfs;
    constexpr_strcpy(entry.name, name);
    entry.filetype = filetype;
    entry.size = 0;

    auto &ctx = *reinterpret_cast<SysfsDirectoryEntryContext*>(entry.opaque);
    ctx.file_id = file_id;

    return entry;
}

static Error get_root_directory(Filesystem&, DirectoryEntry &out_entry)
{
    out_entry = ROOT_DIRECTORY_ENTRIES[0];
    return Success;
}

static Error open(DirectoryEntry &entry, File &out_file)
{
    out_file.fs = entry.fs;
    out_file.refcount = 0;
    out_file.filetype = entry.filetype;
    out_file.read = NULL;
    out_file.write = NULL;
    out_file.seek = NULL;
    out_file.close = [](File &file) { return kfree(file.opaque); };

    auto &entryctx = *reinterpret_cast<SysfsDirectoryEntryContext*>(entry.opaque);

    SysfsOpenFileContext *filectx;
    TRY(kmalloc(sizeof(*filectx), filectx));
    filectx->file_id = entryctx.file_id;
    out_file.opaque = filectx;

    switch (entryctx.file_id) {
    case SysfsFileId::FsRoot:
        out_file.read = [](File&, uint64_t offset, uint8_t *buffer, uint32_t size, uint32_t &bytes_read) {
            if (offset % sizeof(DirectoryEntry) != 0) {
                bytes_read = 0;
                return Success;
            }

            size = round_down<uint32_t>(size, sizeof(DirectoryEntry));
            const uint64_t start_offset = min<uint64_t>(offset, sizeof(ROOT_DIRECTORY_ENTRIES));
            const uint64_t end_offset = min<uint64_t>(offset + size, sizeof(ROOT_DIRECTORY_ENTRIES));
            size = end_offset - start_offset;

            memcpy(buffer, &ROOT_DIRECTORY_ENTRIES[offset / sizeof(DirectoryEntry)], size);
            bytes_read = size;
            return Success;
        };
        break;

    case SysfsFileId::MachineId:
        out_file.read = [](File&, uint64_t, uint8_t *buffer, uint32_t size, uint32_t &bytes_read) {
            if (size == 0) {
                bytes_read = 0;
                return Success;
            }
            
            const char *machine_id = "Raspberry Pi Zero";
            auto to_copy = min<uint32_t>(size, strlen(machine_id));
            memcpy(buffer, machine_id, to_copy - 1);
            buffer[to_copy] = '\0';
            bytes_read = to_copy;
            return Success;
        };
        break;

    case SysfsFileId::CurrentTime:
        out_file.read = [](File&, uint64_t, uint8_t *buffer, uint32_t size, uint32_t &bytes_read) {
            if (size < 8) {
                bytes_read = 0;
                return Success;
            }
            
            uint64_t timestamp = systimer_get_ticks();
            *reinterpret_cast<uint64_t*>(buffer) = timestamp;
            bytes_read = 8;
            return Success;
        };
        break;

    case SysfsFileId::Keyboard:
        out_file.read = [](File&, uint64_t, uint8_t *buffer, uint32_t size, uint32_t &bytes_read) {
            if (size < sizeof(api::KeyEvent)) {
                bytes_read = 0;
                return Success;
            }
            size = sizeof(api::KeyEvent);
            
            bytes_read = 0;
            if (read_keyevent(*reinterpret_cast<api::KeyEvent*>(buffer)))
                bytes_read = sizeof(api::KeyEvent);
            return Success;
        };
        break;

    default:
        return NotSupported;
    }

    return Success;
}

Error sysfs_init(Filesystem* &out_fs)
{
    ROOT_DIRECTORY_ENTRIES[0] = make_dirent(SysfsFileId::FsRoot, "", api::Directory);
    ROOT_DIRECTORY_ENTRIES[1] = make_dirent(SysfsFileId::MachineId, "id", api::CharacterDevice);
    ROOT_DIRECTORY_ENTRIES[2] = make_dirent(SysfsFileId::CurrentTime, "time", api::CharacterDevice);
    ROOT_DIRECTORY_ENTRIES[3] = make_dirent(SysfsFileId::Keyboard, "kbd", api::CharacterDevice);

    out_fs = &sysfs;
    return Success;
};

}
