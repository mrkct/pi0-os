#pragma once

#include <kernel/drivers/device.h>
#include <kernel/vfs/vfs.h>

#include "fat32_structures.h"


int fat32_try_create(BlockDevice &storage, Filesystem **out_fs);
