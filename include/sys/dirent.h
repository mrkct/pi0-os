#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dirent {
    uint64_t    d_ino;
#define DT_UNKNOWN  0U
#define DT_FIFO     1U
#define DT_CHR      2U
#define DT_DIR      4U
#define DT_BLK      6U
#define DT_REG      8U
#define DT_LNK      10U
#define DT_SOCK     12U
#define DT_WHT      14U
    uint8_t     d_type;
    char        d_name[256];
};

/* Convert between stat structure types and directory types.  */
# define IFTODT(mode)       (((mode) & 0170000) >> 12)
# define DTTOIF(dirtype)    ((dirtype) << 12)

struct DIR {
    int             fd;
    struct dirent   *ent;
};
typedef struct DIR DIR;

#ifdef __cplusplus
}
#endif

#endif
