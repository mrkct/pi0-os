#include "fs.h"


uint64_t default_checked_seek(uint64_t filesize, uint64_t current, int whence, int32_t offset)
{
    // FIXME: This doesn't handle overflow very well
    uint64_t newoff;
    switch (whence) {
    case SEEK_SET:
        newoff = offset;
        break;
    case SEEK_CUR:
        newoff = current + offset;
        break;
    case SEEK_END:
        newoff = ((int64_t) filesize) - offset;
        break;
    }
    return clamp<uint64_t>(0, newoff, filesize);
}
