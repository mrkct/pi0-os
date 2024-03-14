#pragma once

#include <kernel/vfs/filesystem.h>
#include <kernel/device/storage.h>


namespace kernel {

Error fat32_create(Filesystem& fs, Storage& storage);

}
