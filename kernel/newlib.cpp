#include <kernel/base.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


extern "C" {

void _exit(int status)
{
    panic("Exiting with status %d", status);
}

// NOTE: _sbrk is implemented in kheap.cpp

int _fstat(int file, struct stat* st)
{
    (void) file;
    (void) st;

    panic("Unimplemented");
    return 0;
}

int _isatty(int file)
{
    (void) file;

    panic("Unimplemented");
    return file < STDERR_FILENO;
}

int _lseek(int file, int ptr, int dir)
{
    (void) file;
    (void) ptr;
    (void) dir;

    panic("Unimplemented");
    return 0;
}

int _link(char const* oldpath, char const* newpath)
{
    (void) oldpath;
    (void) newpath;

    panic("Unimplemented");

    return -1;
}

int _unlink(char const* pathname)
{
    (void)pathname;

    panic("Unimplemented");

    return -1;
}

void _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;

    panic("Unimplemented");

    return;
}

int _getpid(void)
{
    panic("Unimplemented");

    return (int) 0;
}

int _open(char const* pathname, int flags, int mode)
{
    (void) pathname;
    (void) flags;
    (void) mode;

    panic("Unimplemented");

    return -1;
}

int _close(int file)
{
    (void) file;
    panic("Unimplemented");

    return -1;
}

int _write(int file, char* ptr, int len)
{
    (void) file;
    (void) ptr;
    (void) len;

    panic("Unimplemented");
    return -1;
}

int _read(int file, char* ptr, int len)
{
    (void) file;
    (void) ptr;
    (void) len;

    panic("Unimplemented");

    return -1;
}

int mkdir(char const* pathname, mode_t mode)
{
    (void) pathname;
    (void) mode;
    
    panic("Unimplemented");

    return -1;
}

}
