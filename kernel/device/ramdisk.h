#pragma once

#include <kernel/device/storage.h>


namespace kernel {

bool ramdisk_probe();

Error ramdisk_init(Storage&);

}