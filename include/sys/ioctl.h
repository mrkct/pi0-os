#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H


#ifdef __cplusplus
extern "C" {
#endif

int ioctl (int __fd, unsigned long int __request, ...);

#ifdef __cplusplus
}
#endif

#endif
