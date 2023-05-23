#include <kernel/filesystem/filesystem.h>
#include <kernel/lib/string.h>

namespace kernel {

static size_t next_path_separator(char const* path, size_t offset)
{
    size_t i = offset;
    while (path[i] != '/' && path[i] != '\0')
        ++i;
    return i;
}

Error fs_open(Filesystem& fs, char const* path, File& file)
{
    if (path[0] != '/')
        return BadParameters;

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
            if (entry.type != DirectoryEntry::Type::File)
                return NotAFile;

            return fs.open_file_entry(entry, file);
        }
    } while (true);

    return NotFound;
}

Error fs_read(File& file, uint8_t* buffer, size_t offset, size_t size, size_t& bytes_read)
{
    return file.fs->read(file, buffer, offset, size, bytes_read);
}

Error fs_close(File& file)
{
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
