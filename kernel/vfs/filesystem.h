#pragma once

#include <stdint.h>
#include <stddef.h>
#include <cstdint>

#include <api/files.h>
#include <kernel/error.h>


namespace kernel {

constexpr size_t FS_MAX_PATH_LENGTH = 256;

struct File;
struct DirectoryEntry;


struct Filesystem {
    bool is_case_sensitive = true;
    void *opaque;

    Error (*root_directory)(Filesystem&, DirectoryEntry&);
    Error (*open)(DirectoryEntry&, File&);
};

struct File {
    Filesystem *fs;
    uint32_t refcount;
    api::FileType filetype;
    uint64_t size;
    void *opaque;

    Error (*read)(File&, uint64_t, uint8_t*, uint32_t, uint32_t&);
    Error (*write)(File&, uint64_t, uint8_t const*, uint32_t, uint32_t&);
    Error (*seek)(File&, uint64_t current, api::FileSeekMode, int32_t off, uint64_t&);
    Error (*close)(File&);
};

struct DirectoryEntry {
    Filesystem *fs;
    char name[64];
    api::FileType filetype;
    uint64_t size;

    uint8_t opaque[32];
};

}
