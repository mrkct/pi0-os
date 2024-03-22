#ifndef API_FILES_H
#define API_FILES_H

#include <stdint.h>

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

#ifdef __cplusplus
}
#endif

#endif
