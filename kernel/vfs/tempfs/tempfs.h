#pragma once

#include <kernel/vfs/fs.h>
#include <kernel/vfs/vfs.h>


int tempfs_create(Filesystem **out_fs, size_t capacity);
