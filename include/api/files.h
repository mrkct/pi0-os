#ifndef API_FILES_H
#define API_FILES_H

#include <stdint.h>
#include <fcntl.h>
#include "syscalls.h"

#ifdef __cplusplus
namespace api {
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0x200000
#endif

#ifdef __cplusplus
}
#endif

#endif
