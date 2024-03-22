#pragma once

#include <stdint.h>
#include <stddef.h>
#include <api/files.h>
#include <kernel/error.h>
#include "filesystem.h"


namespace kernel {

struct FileCustody {
    File *file;

    uint32_t flags;
    uint64_t seek_position;
};

Error vfs_mount(const char *path, Filesystem*);

Error vfs_open(const char *path, uint32_t flags, FileCustody&);

Error vfs_read(FileCustody&, uint8_t *buffer, uint32_t size, uint32_t&);

Error vfs_write(FileCustody&, uint8_t const *buffer, uint32_t size, uint32_t&);

Error vfs_seek(FileCustody&, api::FileSeekMode, int32_t);

Error vfs_close(FileCustody&);

Error vfs_stat(const char *path, api::Stat&);

Error vfs_duplicate_custody(FileCustody&, FileCustody&);

Error vfs_init();

void vfs_get_default_stdin_stdout_stderr(FileCustody &stdin, FileCustody &stdout, FileCustody &stderr);

}
