#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dirent {
    uint64_t    d_ino;
    char        d_name[256];
};

struct DIR {
    int             fd;
    struct dirent   *ent;
};
typedef struct DIR DIR;

#ifdef __cplusplus
}
#endif

#endif
