#pragma once

#include <kernel/vfs/fs.h>
#include <kernel/vfs/vfs.h>


int pipefs_create(Filesystem **out_fs);
