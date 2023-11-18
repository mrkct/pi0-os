#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <api/syscalls.h>


static int sys_debuglog(char const *str, int len)
{
    if (0 == syscall(SYS_DebugLog, (uint32_t) str, len, 0, 0, 0))
        return len;
    return -1;
}

void _exit(int status)
{
    syscall(SYS_Exit, status, 0, 0, 0, 0);
}

static uint8_t s_heap[16 * 1024 * 1024];
static uint8_t *s_brk = &s_heap;
int(*s_stdout_print_func)(char const *, int) = sys_debuglog;
int(*s_stderr_print_func)(char const *, int) = sys_debuglog;

void* _sbrk(int incr)
{
    uint8_t *brk = s_brk;
    s_brk += incr;
    return brk;
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
