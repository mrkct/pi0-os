#pragma once

#include <kernel/filesystem/storage.h>

namespace kernel {

bool ramdisk_probe();

Error ramdisk_init(Storage&);

}