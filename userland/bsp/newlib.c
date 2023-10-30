#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syscalls.h>


void _exit(int status)
{
    syscall(SYS_Exit, status, 0, 0, 0, 0);
}

static uint8_t s_heap[4096];
static uint8_t *s_brk = &s_heap;

void* _sbrk(int incr)
{
    s_brk += incr;
    return s_brk;
}

int _fstat(int file, struct stat* st)
{
    st->st_mode = S_IFCHR;

    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _link(char const* oldpath, char const* newpath)
{
    return -1;
}

int _unlink(char const* pathname)
{
    return -1;
}

void _kill(int pid, int sig)
{
    return;
}

int _getpid(void)
{
    ProcessInfo info;
    syscall(SYS_GetProcessInfo, (uint32_t) &info, 0, 0, 0, 0);

    return (int) info.pid;
}

int _open(char const* pathname, int flags, int mode)
{
    if (flags & ~(O_RDONLY | O_RDWR | O_WRONLY | O_APPEND) != 0)
        return -1;

    int rc = syscall(
        SYS_OpenFile,
        (uint32_t) pathname,
        strlen(pathname),
        MODE_READ | MODE_WRITE,
        0, 0
    );

    if (rc > 0 && (flags & O_APPEND))
        _lseek(rc, 0, 2);
    
    return rc;
}

int _close(int file)
{
    if (file <= 2)
        return -1;

    return syscall(SYS_CloseFile, (uint32_t) file, 0, 0, 0, 0);
}

int _write(int file, char* ptr, int len)
{
    if (file == 1 || file == 2) {
        return syscall(SYS_DebugLog, (uint32_t) ptr, len, 0, 0, 0);
    } else if (file == 0) {
        return -1;
    }

    return -1;
}

int _read(int file, char* ptr, int len)
{
    return syscall(
        SYS_ReadFile,
        (uint32_t)file,
        (uint32_t) ptr,
        len,
        0, 0
    );
}

int mkdir(char const* pathname, mode_t mode)
{
    return -1;
}
