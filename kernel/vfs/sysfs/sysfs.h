#pragma once

#include <kernel/vfs/filesystem.h>


namespace kernel {

Error sysfs_init(Filesystem*&);

}