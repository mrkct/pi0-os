#include <kernel/filesystem/filesystem.h>
#include <kernel/lib/math.h>
#include <kernel/lib/string.h>

namespace kernel {

static size_t next_path_separator(char const* path, size_t offset)
{
    size_t i = offset;
    while (path[i] != '/' && path[i] != '\0')
        ++i;
    return i;
}

static Filesystem* g_root_fs;

void fs_set_root(Filesystem* fs)
{
    kassert(fs != nullptr);
    kassert(g_root_fs == nullptr);
    g_root_fs = fs;
}

Filesystem* fs_get_root()
{
    return g_root_fs;
}

static Error fs_get_directory_entry(Filesystem& fs, char const* path, DirectoryEntry& dirent)
{
    if (path[0] != '/')
        return BadParameters;

    if (klib::strcmp(path, "/") == 0) {
        dirent = DirectoryEntry {
            .dir = nullptr,
            .name = { '/', '\0' },
            .type = DirectoryEntry::Type::Directory,
            .size = 0,
            .impl_data = {},
        };
        return Success;
    }

    auto const& starts_with = [](char const* str, char const* prefix) {
        while (*prefix != '\0') {
            if (*str == '\0' || *str != *prefix)
                return false;
            ++str;
            ++prefix;
        }
        return true;
    };

    Directory dir;
    TRY(fs.root_directory(fs, dir));

    const size_t path_len = klib::strlen(path);
    size_t start_of_name = 1;
    size_t end_of_name = next_path_separator(path, 1);

    while (end_of_name < path_len) {
        DirectoryEntry entry;
        do {
            auto err = fs.directory_next_entry(dir, entry);
            if (!err.is_success()) {
                if (err.generic_error_code == GenericErrorCode::EndOfData)
                    return NotFound;
                return err;
            }

            if (starts_with(path + start_of_name, entry.name)) {
                if (entry.type != DirectoryEntry::Type::Directory)
                    return NotADirectory;

                TRY(fs.open_directory_entry(entry, dir));
                break;
            }
        } while (true);

        start_of_name = end_of_name + 1;
        end_of_name = next_path_separator(path, start_of_name);
    }

    DirectoryEntry entry;
    do {
        auto err = fs.directory_next_entry(dir, entry);
        if (!err.is_success()) {
            if (err.generic_error_code == GenericErrorCode::EndOfData)
                return NotFound;
            return err;
        }

        if (klib::strcmp(entry.name, path + start_of_name) == 0) {
            dirent = entry;
            return Success;
        }
    } while (true);
}

void file_inc_ref(File& file)
{
    kassert(file.fs != nullptr);
    kassert(file.fs->storage != nullptr);
    file.ref_count++;
}

void file_dec_ref(File& file)
{
    kassert(file.fs != nullptr);
    kassert(file.fs->storage != nullptr);
    kassert(file.ref_count > 0);
    file.ref_count--;
    if (file.ref_count == 0) {
        // TODO: free file
    }
}

Error fs_stat(Filesystem& fs, char const* path, Stat& stat)
{
    DirectoryEntry entry;
    TRY(fs_get_directory_entry(fs, path, entry));
    stat = Stat {
        .is_directory = entry.type == DirectoryEntry::Type::Directory,
        .size = entry.size
    };

    return Success;
}

Error fs_open(Filesystem& fs, char const* path, File& file)
{
    DirectoryEntry entry;
    TRY(fs_get_directory_entry(fs, path, entry));

    if (entry.type != DirectoryEntry::Type::File)
        return NotAFile;

    TRY(fs.open_file_entry(entry, file));
    file_inc_ref(file);

    return Success;
}

Error fs_read(File& file, uint8_t* buffer, size_t offset, size_t size, size_t& bytes_read)
{
    return file.fs->read(file, buffer, offset, size, bytes_read);
}

Error fs_seek(File& file, int64_t offset, SeekMode mode)
{
    switch (mode) {
    case SeekMode::Current:
        if (offset < 0 && static_cast<uint64_t>(klib::abs(offset)) > file.current_offset) {
            file.current_offset = 0;
        } else if (file.size - file.current_offset < static_cast<uint64_t>(offset)) {
            file.current_offset = file.size;
        } else {
            file.current_offset += offset;
        }
        break;
    case SeekMode::Start:
        if (offset < 0) {
            file.current_offset = 0;
        } else if (static_cast<uint64_t>(offset) > file.size) {
            file.current_offset = file.size;
        } else {
            file.current_offset = offset;
        }
        break;
    case SeekMode::End:
        if (offset > 0) {
            file.current_offset = file.size;
        } else if (static_cast<uint64_t>(klib::abs(offset)) > file.size) {
            file.current_offset = 0;
        } else {
            file.current_offset += offset;
        }
        break;
    }

    return Success;
}

Error fs_close(File& file)
{
    file_dec_ref(file);
    return Success;
}

Error fs_open_directory(Filesystem& fs, char const* path, Directory& directory)
{
    if (klib::strcmp(path, "/") == 0)
        return fs.root_directory(fs, directory);

    return Success;
}

Error fs_directory_next_entry(Directory& dir, DirectoryEntry& entry)
{
    return dir.fs->directory_next_entry(dir, entry);
}

}
