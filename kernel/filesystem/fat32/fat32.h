#pragma once

#include <kernel/error.h>
#include <kernel/filesystem/filesystem.h>
#include <kernel/filesystem/storage.h>
#include <stddef.h>
#include <stdint.h>

namespace kernel {

Error fat32_create(Filesystem& fs, Storage& storage);

}
