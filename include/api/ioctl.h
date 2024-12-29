#ifndef API_IOCTL_H
#define API_IOCTL_H

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
namespace api {
#endif

enum FramebufferIoctl {
    FBIO_GET_DISPLAY_INFO = 1,
    FBIO_REFRESH = 2,
    FBIO_MAP = 3,
};

#ifdef __cplusplus
}
#endif

#endif