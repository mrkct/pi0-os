#pragma once

#include <kernel/vfs/vfs.h>


namespace kernel {

Error create_pipe(FileCustody &out_readend, FileCustody &out_writeend);

}