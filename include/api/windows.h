#ifndef API_WINDOW_H
#define API_WINDOW_H

#include <stdint.h>
#include <stddef.h>
#include "syscalls.h"


#ifdef __cplusplus
namespace api {
#endif

static inline int sys_create_window(uint32_t width, uint32_t height)
{
    return syscall(SYS_CreateWindow, NULL, width, height, 0, 0, 0);
}

static inline int sys_refresh_window(uint32_t *framebuffer)
{
    return syscall(SYS_UpdateWindow, NULL, (uint32_t) framebuffer, 0, 0, 0, 0);
}

#ifdef __cplusplus
}
#endif

#endif