#include "filesystem.h"
#include <kernel/lib/math.h>


namespace kernel {

Error normal_checked_seek(File &file, uint64_t current, api::FileSeekMode mode, int32_t off, uint64_t &out_seek)
{
    uint64_t seek = current;
    switch (mode) {
    case api::SeekStart:
        seek = off;
        break;
    case api::SeekCurrent:
        seek += off;
        break;
    case api::SeekEnd:
        seek = (uint64_t)((int64_t) file.size + off);
        break;
    }
    out_seek = clamp<uint64_t>(0, seek, file.size);
    return Success;
}

}
