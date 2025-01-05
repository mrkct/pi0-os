#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 * These definitions are the same as the one in errno.h ,
 * except they have a slightly different name to disambiguate
 */
typedef enum Errors {
    ERR_PERM = 1,	    /* Not owner */
    ERR_NOENT = 2,	    /* No such file or directory */
    ERR_IO = 5,		    /* I/O error */
    ERR_2BIG = 7,	    /* Arg list too long */
    ERR_NOEXEC = 8,	    /* Exec format error */
    ERR_BADF = 9,       /* Bad file number */
    ERR_NOMEM =	12,	    /* Not enough space */
    ERR_BUSY = 16,	    /* Device or resource busy */
    ERR_NODEV = 19,	    /* No such device */
    ERR_NOTDIR = 20,	/* Not a directory */
    ERR_ISDIR = 21,	    /* Is a directory */
    ERR_INVAL = 22,	    /* Invalid argument */
    ERR_NFILE = 23,	    /* Too many open files in system */
    ERR_ROFS = 30,	    /* Read-only file system */
    ERR_PIPE = 32,	    /* Broken pipe */
    ERR_NOSYS = 88,	    /* Function not implemented */
    ERR_TIMEDOUT = 116,	/* Connection timed out */
    ERR_ALREADY = 120,	/* Socket already connected */
    ERR_NOTSUP = 134	/* Not supported */
} Errors;

#if __SIZEOF_POINTER__ == 4
    typedef uint32_t sysarg_t;
#elif __SIZEOF_POINTER__ == 8
    typedef uint64_t sysarg_t;
#endif

typedef enum SyscallIdentifiers {
    SYS_Yield = 1,
    SYS_Exit = 2,
    SYS_GetPid = 4,
    SYS_Fork = 5,
    SYS_Execve = 6,
    SYS_WaitPid = 7,
    SYS_Kill = 8,
    SYS_Poll = 9,

    SYS_Open = 10,
    SYS_Read = 11,
    SYS_Write = 12,
    SYS_Close = 13,
    SYS_Ioctl = 14,
    SYS_FStat = 15,
    SYS_Seek = 16,
    SYS_CreatePipe = 17,
    SYS_MoveFd = 18,

    SYS_Link = 19,
    SYS_Unlink = 20,
    SYS_MakeDirectory = 21,

    SYS_MilliSleep = 31,
} SyscallIdentifiers;

typedef enum MajorDeviceNumber {
    Maj_Reserved = 0,
    Maj_Disk = 3,
    Maj_TTY = 4,
    Maj_Console = 5,
    Maj_UART = 6,
    Maj_GPIO = 7,
    Maj_RTC = 8,
    Maj_Keyboard = 9,
    Maj_Mouse = 10,
    Maj_Input = 11,
    Maj_Framebuffer = 12,
} MajorDeviceNumber;

#include "arm/syscall.h"

/* Process */

#ifdef __cplusplus
namespace api {
#endif

static inline void sys_exit(int status)
{
    syscall(SYS_Exit, (sysarg_t) status, 0, 0, 0);
}

static inline void sys_yield()
{
    syscall(SYS_Yield, 0, 0, 0, 0);
}

static inline int sys_fork()
{
    return syscall(SYS_Fork, 0, 0, 0, 0);
}

static inline int sys_execve(const char *pathname, const char *const argv[], const char *const envp[])
{
    return syscall(SYS_Execve, (sysarg_t) pathname, (sysarg_t) argv, (sysarg_t) envp, 0);
}

static inline int sys_getpid()
{
    return syscall(SYS_GetPid, 0, 0, 0, 0);
}

static inline void sys_kill(int pid, int sig)
{
    syscall(SYS_Kill, (sysarg_t) pid, (sysarg_t) sig, 0, 0);
}

static inline int sys_waitpid(int pid, int *status, int options)
{
    return syscall(SYS_WaitPid, (sysarg_t) pid, (sysarg_t) status, (sysarg_t) options, 0);
}

static inline int sys_mkpipe(int *writer, int *receiver)
{
    return syscall(SYS_CreatePipe, (sysarg_t) writer, (sysarg_t) receiver, 0, 0);
}

static inline int sys_movefd(int oldfd, int newfd)
{
    return syscall(SYS_MoveFd, (sysarg_t) oldfd, (sysarg_t) newfd, 0, 0);
}

typedef struct PollFd {
    int32_t fd;

#define F_POLLMASK        (F_POLLIN | F_POLLOUT)
#define F_POLLIN          0x001   /* There is data to read.  */
#define F_POLLOUT         0x004   /* Writing now will not block.  */
    uint32_t events;
    uint32_t revents;
} PollFd;

static inline int sys_poll(PollFd *fds, int nfds, int timeout)
{
    return syscall(SYS_Poll, (sysarg_t) fds, (sysarg_t) nfds, (sysarg_t) timeout, 0);
}

#ifdef __cplusplus
}
#endif

/* Time */

#ifdef __cplusplus
namespace api {
#endif

typedef struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} DateTime;

static inline int datetime_compare(const DateTime *a, const DateTime *b)
{
    if (a->year != b->year)
        return a->year - b->year;
    if (a->month != b->month)
        return a->month - b->month;
    if (a->day != b->day)
        return a->day - b->day;
    if (a->hour != b->hour)
        return a->hour - b->hour;
    if (a->minute != b->minute)
        return a->minute - b->minute;
    if (a->second != b->second)
        return a->second - b->second;
    return 0;
}

typedef struct TimeSpec {
    uint32_t seconds;
    uint32_t nanoseconds;
} TimeSpec;

static inline int sys_millisleep(int millis)
{
    return syscall(SYS_MilliSleep, (sysarg_t) millis, 0, 0, 0);
}

#ifdef __cplusplus
}
#endif

/* Files */

#ifdef __cplusplus
namespace api {
#endif

/* Supported SYS_Open flags */
#define OF_ACCMODE      (OF_RDONLY | OF_WRONLY | OF_RDWR)
#define OF_RDONLY       0x0000
#define OF_WRONLY       0x0001
#define OF_RDWR         0x0002
#define OF_APPEND       0x0008
#define OF_CREATE       0x0200
#define OF_DIRECTORY    0x200000

typedef struct Stat 
{
  uint32_t	    st_dev;
  uint64_t	    st_ino;

#define SF_IFDIR        0040000
#define SF_IFCHR        0020000
#define SF_IFBLK        0060000
#define SF_IFREG        0100000
#define SF_IFLNK        0120000
#define SF_IFSOCK       0140000
#define SF_IFIFO        0010000
  uint32_t	    st_mode;
  uint32_t	    st_nlink;
  uint32_t	    st_uid;
  uint32_t	    st_gid;
  uint32_t	    st_rdev;
  uint64_t	    st_size;
  /**
   * Note: these fields also originally had the st_ prefix, but
   * I removed it because <sys/stat.h> also contains macros
   * that alias "st_atime" to "st_atim.tv_sec", etc.
   */
  TimeSpec      atim;
  TimeSpec      mtim;
  TimeSpec      ctim;
  uint64_t      st_blksize;
  uint64_t      st_blocks;
} Stat;

static inline int sys_open(const char *path, int flags, int mode)
{
    return syscall(SYS_Open, (sysarg_t) path, (sysarg_t) flags, (sysarg_t) mode, 0);
}

static inline int sys_read(int fd, char *buf, size_t count)
{
    return syscall(SYS_Read, (sysarg_t) fd, (sysarg_t) buf, (sysarg_t) count, 0);
}

static inline int sys_write(int fd, const char *buf, size_t count)
{
    return syscall(SYS_Write, (sysarg_t) fd, (sysarg_t) buf, (sysarg_t) count, 0);
}

static inline int sys_close(int fd)
{
    return syscall(SYS_Close, (sysarg_t) fd, 0, 0, 0);
}

static inline int sys_ioctl(int fd, unsigned long request, void *arg)
{
    return syscall(SYS_Ioctl, (sysarg_t) fd, (sysarg_t) request, (sysarg_t) arg, 0);
}

static inline int sys_seek(int fd, int offset, int whence, uint64_t *out_new_offset)
{
    return syscall(SYS_Seek, (sysarg_t) fd, (sysarg_t) offset, (sysarg_t) whence, (sysarg_t) out_new_offset);
}

static inline int sys_fstat(int fd, Stat *buf)
{
    return syscall(SYS_FStat, (sysarg_t) fd, (sysarg_t) buf, 0, 0);
}

static inline int sys_link(const char *oldpath, const char *newpath)
{
    return syscall(SYS_Link, (sysarg_t) oldpath, (sysarg_t) newpath, 0, 0);
}

static inline int sys_unlink(const char *path)
{
    return syscall(SYS_Unlink, (sysarg_t) path, 0, 0, 0);
}

static inline int sys_mkdir(const char *path, int mode)
{
    return syscall(SYS_MakeDirectory, (sysarg_t) path, (sysarg_t) mode, 0, 0);
}

#ifdef __cplusplus
}
#endif


/* Ioctl */

#ifdef __cplusplus
namespace api {
#endif

typedef struct FramebufferDisplayInfo {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bytes_per_pixel;
} FramebufferDisplayInfo;

enum FramebufferIoctl {
    FBIO_GET_DISPLAY_INFO = 1,
    FBIO_REFRESH = 2,
    FBIO_MAP = 3,
};

enum RealTimeClockIoctl {
    RTCIO_GET_DATETIME = 1,
};


#ifdef __cplusplus
}
#endif
