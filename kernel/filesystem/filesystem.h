#pragma once

#include <kernel/error.h>
#include <kernel/filesystem/storage.h>
#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct File;
struct Directory;
struct DirectoryEntry;

struct Filesystem {
    Error (*init)(Filesystem&);
    Error (*root_directory)(Filesystem&, Directory&);
    Error (*directory_next_entry)(Directory&, DirectoryEntry&);
    Error (*open_file_entry)(DirectoryEntry&, File&);
    Error (*open_directory_entry)(DirectoryEntry&, Directory&);
    Error (*read)(File&, uint8_t* buffer, size_t offset, size_t size, size_t& bytes_read);

    Storage* storage;
    void* impl_data;
};

struct File {
    Filesystem* fs;
    char name[256];
    uint64_t size;

    uint8_t impl_data[32];
};

struct Directory {
    Filesystem* fs;
    uint8_t impl_data[32];
};

struct DirectoryEntry {
    Directory* dir;

    char name[256];
    enum class Type {
        File,
        Directory
    } type;
    uint8_t impl_data[32];
};

Error fs_open(Filesystem&, char const* path, File& file);

Error fs_read(File& file, uint8_t* buffer, size_t offset, size_t size, size_t& bytes_read);

Error fs_close(File& file);

Error fs_open_directory(Filesystem&, char const* path, Directory& directory);

Error fs_directory_next_entry(Directory&, DirectoryEntry& entry);

template<typename Callback>
Error fs_directory_for_each_entry(Directory& directory, Callback callback)
{
    DirectoryEntry entry;
    Error err;
    while ((err = fs_directory_next_entry(directory, entry)).is_success()) {
        callback(entry);
    }
    return err.generic_error_code == GenericErrorCode::EndOfData ? Success : err;
}

template<typename Callback>
Error fs_directory_for_each_entry(Filesystem& fs, char const* path, Callback callback)
{
    Directory directory;
    Error err = fs_open_directory(fs, path, directory);
    if (!err.is_success())
        return err;
    return fs_directory_for_each_entry(directory, callback);
}

}