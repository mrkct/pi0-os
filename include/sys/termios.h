#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H

#include <stdint.h>
#include <api/syscalls.h>

#ifdef __cplusplus
extern "C" {
using namespace api;

#endif

typedef uint32_t tcflag_t;
typedef uint8_t cc_t;
typedef uint32_t speed_t;

typedef struct termios {

/* c_iflag bits */
#define INLCR	0000100  /* Map NL to CR on input.  */
#define ICRNL	0000400    /* Map CR to NL on input.  */
    tcflag_t c_iflag;      /* input modes */
#define ONLCR	0000004    /* Map NL to CR-NL on output.  */
    tcflag_t c_oflag;      /* output modes */
    tcflag_t c_cflag;      /* control modes */

/* c_lflag bits */
#define ISIG	0000001    /* Enable signals.  */
#define ICANON	0000002    /* Canonical input (erase and kill processing).  */
#define ECHO	0000010    /* Enable echo.  */
#define ECHOE	0000020    /* Echo erase character as error-correcting backspace.  */
#define ECHOK	0000040    /* Echo KILL.  */
#define ECHONL	0000100    /* Echo NL.  */
    tcflag_t c_lflag;      /* local modes */
#define NCCS 32
    cc_t     c_cc[NCCS];   /* special characters */
} termios;

static inline int tcgetattr(int fd, struct termios *termios_p)
{
    return sys_ioctl(fd, TTYIO_TCGETATTR, (void*) termios_p);
}

static inline int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    (void) optional_actions;
    return sys_ioctl(fd, TTYIO_TCSETATTR, (void*) termios_p);
}

#ifdef __cplusplus
}
#endif


#endif
