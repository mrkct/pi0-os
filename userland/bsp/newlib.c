#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <api/syscalls.h>


static int sys_debuglog(char const *str, int len)
{
    if (0 == syscall(SYS_DebugLog, NULL, (uint32_t) str, len, 0, 0, 0))
        return len;
    return -1;
}

void _exit(int status)
{
    syscall(SYS_Exit, NULL, status, 0, 0, 0, 0);
    while(1);
}

static uint8_t s_heap[16 * 1024 * 1024];
static uint8_t *s_brk = s_heap;
int(*s_stdout_print_func)(char const *, int) = sys_debuglog;
int(*s_stderr_print_func)(char const *, int) = sys_debuglog;

static const char *OUT_OF_MEMORY_ERROR_MSG = "\n"
    "@@@@@@@@@@@@@@@@@@@@@@@@@\n"
    "@@@@@ OUT OF MEMORY @@@@@\n"
    "@@@@@@@@@@@@@@@@@@@@@@@@@\n";

void* _sbrk(int incr)
{
    uint8_t *brk = s_brk;
    if (s_brk + incr > s_heap + sizeof(s_heap)) {
        write(2, OUT_OF_MEMORY_ERROR_MSG, sizeof(OUT_OF_MEMORY_ERROR_MSG));
        exit(-1);
        return NULL;
    }
    s_brk += incr;
    return brk;
}

int _fstat(int file, struct stat* st)
{
    (void) file;
    (void) st;
    st->st_mode = S_IFCHR;

    return 0;
}

int _isatty(int file)
{
    (void)file;

    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;

    return 0;
}

int _link(char const* oldpath, char const* newpath)
{
    (void)oldpath;
    (void)newpath;

    return -1;
}

int _unlink(char const* pathname)
{
    (void)pathname;
    return -1;
}

void _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return;
}

int _getpid(void)
{
    ProcessInfo info;
    syscall(SYS_GetProcessInfo, NULL, (uint32_t) &info, 0, 0, 0, 0);

    return (int) info.pid;
}

int _open(char const* pathname, int flags, int mode)
{
    (void) mode;

    if ((flags & ~(O_RDONLY | O_RDWR | O_WRONLY | O_APPEND)) != 0)
        return -1;

    uint32_t fd;
    int rc = syscall(
        SYS_OpenFile,
        (uint32_t*)&fd,
        (uint32_t) pathname,
        strlen(pathname),
        MODE_READ | MODE_WRITE,
        0, 0
    );

    if (rc > 0 && (flags & O_APPEND))
        _lseek(rc, 0, 2);
    
    return (int) fd;
}

int _close(int file)
{
    if (file <= 2)
        return -1;

    return syscall(SYS_CloseFile, NULL, (uint32_t) file, 0, 0, 0, 0);
}

int _write(int file, char* ptr, int len)
{
    if (file == 1) {
        return s_stdout_print_func ? s_stdout_print_func(ptr, len) : -1;
    } else if (file == 2) {
        return s_stderr_print_func ? s_stderr_print_func(ptr, len) : -1;
    } else if (file == 0) {
        return -1;
    }

    return -1;
}

int _read(int file, char* ptr, int len)
{
    return syscall(
        SYS_ReadFile,
        NULL,
        (uint32_t) file,
        (uint32_t) ptr,
        len,
        0, 0
    );
}

int mkdir(char const* pathname, mode_t mode)
{
    (void) pathname;
    (void) mode;
    
    return -1;
}
