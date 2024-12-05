#ifndef API_FILES_H
#define API_FILES_H

#include <stdint.h>
#include "syscalls.h"

#ifdef __cplusplus
namespace api {
#endif

typedef enum {
    OPEN_FLAG_READ   = 1 << 0,
    OPEN_FLAG_WRITE  = 1 << 1,
    OPEN_FLAG_APPEND = 2 << 2
} OpenFileFlag;

typedef enum {
    SeekStart,
    SeekCurrent,
    SeekEnd
} FileSeekMode;

typedef enum {
    RegularFile,
    Directory,
    CharacterDevice,
    Pipe
} FileType;

typedef struct {
    FileType type;
    uint64_t size;
} Stat;

typedef struct {
    char name[256];
    FileType filetype;
    uint64_t size;
} DirectoryEntry;

static inline int sys_read_direntry(int fd, DirectoryEntry *out_direntry, size_t count)
{
    uint32_t entries_read;
    int rc = syscall(SYS_ReadDirEntry, &entries_read, fd, (uint32_t) out_direntry, count, 0, 0, 0);
    if (rc != 0) {
        return -rc;
    }
    
    return entries_read;
}

#ifdef __cplusplus
}
#endif

#endif
