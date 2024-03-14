#include "vfs.h"
#include "path.h"


namespace kernel {

static constexpr uint32_t MAX_MOUNTPOINTS = 8;
static constexpr uint32_t MAX_OPEN_FILES = 256;

struct OpenFileEntry {
    File file; // This member should always come first due to implementation reasons
    bool is_entry_valid;
    Path path;
};

static struct {
    struct {
        Path path;
        Filesystem *filesystem;
    } mountpoints[MAX_MOUNTPOINTS];
    uint32_t mountpoints_length;
    OpenFileEntry open_files[MAX_OPEN_FILES];
} s_vfs;

static Error open_dirent(Path path, uint32_t flags, DirectoryEntry entry, FileCustody &out_custody);


static File *lookup_open_files_list(Path path)
{
    for (uint32_t i = 0; i < MAX_OPEN_FILES; i++) {
        auto& that = s_vfs.open_files[i];
        if (that.is_entry_valid && path_compare(path, that.path, that.file.fs->is_case_sensitive))
            return &that.file;
    }

    return NULL;
}

static Error reserve_element_in_open_file_list(Path path, File* &file)
{
    for (uint32_t i = 0; i < MAX_OPEN_FILES; i++) {
        auto& that = s_vfs.open_files[i];
        if (!that.is_entry_valid) {
            that.path = path;
            that.is_entry_valid = true;
            file = &that.file;
            return Success;
        }
    }

    return OutOfMemory;
}

static Error remove_from_open_files_list(File *file)
{
    // NOTE: We can do this because 'file' is the first member of the struct
    OpenFileEntry *entry = (OpenFileEntry*) file;
    if (entry < s_vfs.open_files || (uint32_t)(entry - s_vfs.open_files) >= MAX_OPEN_FILES)
        return NotFound;

    entry->is_entry_valid = false;
    return Success;
}

static Error traverse_to_direntry(Filesystem &filesystem, Path path, DirectoryEntry &out_entry)
{
    if (path.len == 0) {
        TRY(filesystem.root_directory(filesystem, out_entry));
        return Success;
    }

    if (path.str[0] != '/')
        return BadParameters;    

    DirectoryEntry dirent;
    FileCustody directory;
    Path basedir = path_basedir(path);
    Path basename = path_basename(path);
    
    TRY(traverse_to_direntry(filesystem, basedir, dirent));
    TRY(open_dirent(basedir, api::OPEN_FLAG_READ, dirent, directory));

    Error error = Success;
    uint32_t bytes_read;
    do {
        error = vfs_read(directory, (uint8_t*) &dirent, sizeof(DirectoryEntry), bytes_read);
        if (!error.is_success())
            break;
        
        if (bytes_read != sizeof(DirectoryEntry)) {
            error = NotFound;
            break;
        }
    } while (!path_compare(basename, dirent.name, dirent.fs->is_case_sensitive));

    vfs_close(directory);
    if (error.is_success())
        out_entry = dirent;
    
    return error;
}

static Error traverse_to_direntry(Path path, DirectoryEntry &out_entry)
{
    Filesystem *filesystem = NULL;
    Path fs_relative_path;
    for (uint32_t i = 0; i < s_vfs.mountpoints_length; i++) {
        if (path_startswith(path, s_vfs.mountpoints[i].path)) {
            fs_relative_path = path_from_string(path.str + s_vfs.mountpoints[i].path.len);
            filesystem = s_vfs.mountpoints[i].filesystem;
        }
    }

    if (!filesystem)
        return NotFound;
    
    TRY(traverse_to_direntry(*filesystem, fs_relative_path, out_entry));
    return Success;
}

static Error open_dirent(Path path, uint32_t flags, DirectoryEntry entry, FileCustody &out_custody)
{
    File *file;
    TRY(reserve_element_in_open_file_list(path, file));

    if (auto err = entry.fs->open(entry, *file); !err.is_success()) {
        remove_from_open_files_list(file);
        return err;
    }

    out_custody = {
        .file = file, 
        .flags = flags,
        .seek_position = 0
    };

    return Success;
}

Error vfs_mount(const char *stringpath, Filesystem *fs)
{
    Path path = path_from_string(stringpath);
    if (s_vfs.mountpoints_length == MAX_MOUNTPOINTS)
        return OutOfMemory;
    
    // Paths must be stored in reverse-order by path length, otherwise the lookup is harder to do
    size_t pos;
    for (pos = 0; pos < s_vfs.mountpoints_length; pos++) {
        if (s_vfs.mountpoints[pos].path.len < path.len)
            break;
    }

    for (size_t i = s_vfs.mountpoints_length; i > pos; i--) {
        s_vfs.mountpoints[i] = s_vfs.mountpoints[i - 1];
    }
    s_vfs.mountpoints[pos] = {
        .path = path,
        .filesystem = fs
    };
    s_vfs.mountpoints_length++;

    return Success;
}

Error vfs_open(const char *stringpath, uint32_t flags, FileCustody &out_custody)
{
    Path path = path_from_string(stringpath);
    File *file = lookup_open_files_list(path);
    if (file) {
        file->refcount++;
        out_custody = {
            .file = file,
            .flags = flags,
            .seek_position = 0
        };
        return Success;
    }

    DirectoryEntry entry;
    TRY(traverse_to_direntry(path, entry));
    TRY(open_dirent(path, flags, entry, out_custody));

    return Success;
}

Error vfs_read(FileCustody &custody, uint8_t *buffer, uint32_t size, uint32_t &bytes_read)
{
    bytes_read = 0;
    if (!custody.file->read)
        return NotSupported;

    Error result = custody.file->read(*custody.file, custody.seek_position, buffer, size, bytes_read);
    if (result.is_success())
        custody.seek_position += bytes_read;
    return result;
}

Error vfs_write(FileCustody &custody, uint8_t const *buffer, uint32_t size, uint32_t &bytes_written)
{
    bytes_written = 0;
    if (!custody.file->write)
        return NotSupported;
    
    Error result = custody.file->write(*custody.file, custody.seek_position, buffer, size, bytes_written);
    if (result.is_success())
        custody.seek_position += bytes_written;
    return result;
}

Error vfs_seek(FileCustody &custody, api::FileSeekMode mode, int32_t offset)
{
    if (!custody.file->seek)
        return NotSupported;

    uint64_t new_seek;
    TRY(custody.file->seek(*custody.file, custody.seek_position, mode, offset, new_seek));
    custody.seek_position = new_seek;

    return Success;
}

Error vfs_close(FileCustody &custody)
{
    custody.file->refcount--;
    if (custody.file->refcount == 0) {
        if (custody.file->close)
            custody.file->close(*custody.file);
        remove_from_open_files_list(custody.file);
    }

    return Success;
}

Error vfs_stat(const char *path, api::Stat &stat)
{
    DirectoryEntry entry;
    TRY(traverse_to_direntry(path_from_string(path), entry));

    stat = {
        .type = entry.filetype,
        .size = entry.size
    };
    return Success;
}

}
