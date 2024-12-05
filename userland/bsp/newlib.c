#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <api/syscalls.h>


void _exit(int status)
{
    syscall(SYS_Exit, (uint32_t) status, 0, 0, 0, 0, 0);
    while(1);
}

static uint8_t s_heap[64 * 1024 * 1024];
static uint8_t *s_brk = s_heap;
static const char *OUT_OF_MEMORY_ERROR_MSG = "\n"
    "@@@@@@@@@@@@@@@@@@@@@@@@@\n"
    "@@@@@ OUT OF MEMORY @@@@@\n"
    "@@@@@@@@@@@@@@@@@@@@@@@@@\n";

void* _sbrk(int incr)
{
    uint8_t *brk = s_brk;
    if (s_brk + incr > s_heap + sizeof(s_heap)) {
        write(STDERR_FILENO, OUT_OF_MEMORY_ERROR_MSG, sizeof(OUT_OF_MEMORY_ERROR_MSG));
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

    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return 0;
}

int _isatty(int file)
{
    (void) file;
    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return file < STDERR_FILENO;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return -1;
}

int _link(char const* oldpath, char const* newpath)
{
    (void)oldpath;
    (void)newpath;
    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return -1;
}

int _unlink(char const* pathname)
{
    (void)pathname;
    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return -1;
}

void _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return;
}

int _getpid(void)
{
    fprintf(stderr, "%s - not implemented", __PRETTY_FUNCTION__);
    return 0;
}

int _open(char const* pathname, int flags, int mode)
{
    return syscall(SYS_Open, (uint32_t) pathname, (uint32_t) flags, (uint32_t) mode, 0, 0, 0);
}

int _close(int file)
{
    return syscall(SYS_Close, (uint32_t) file, 0, 0, 0, 0, 0);
}

int _write(int file, char* ptr, int len)
{
    return syscall(SYS_Write, (uint32_t) file, (uint32_t) ptr, (uint32_t) len, 0, 0, 0);
}

int _read(int file, char* ptr, int len)
{
    return syscall(SYS_Read, (uint32_t) file, (uint32_t) ptr, (uint32_t) len, 0, 0, 0);
}

int mkdir(char const* pathname, mode_t mode)
{
    (void) pathname;
    (void) mode;
    
    return -1;
}
