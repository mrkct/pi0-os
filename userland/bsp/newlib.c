#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <api/syscalls.h>


void _exit(int status)
{
    syscall(SYS_Exit, (uint32_t) status, 0, 0, 0, 0, 0);
    while(1);
}

static uint8_t s_heap[32 * 1024 * 1024];
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
    return syscall(SYS_FStat, (uint32_t) file, (uint32_t) st, 0, 0, 0, 0);
}

int _isatty(int file)
{
    (void) file;
    return file < STDERR_FILENO;
}

off_t _lseek(int file, int ptr, int dir)
{
    int rc;
    uint64_t offset = 0;
    rc = syscall(SYS_Seek, (uint32_t) file, (uint32_t) ptr, (uint32_t) dir, (uint32_t) &offset, 0, 0);
    if (rc < 0)
        return -1;
    return (off_t) offset;
}

int _link(char const* oldpath, char const* newpath)
{
    return syscall(SYS_Link, (uint32_t) oldpath, (uint32_t) newpath, 0, 0, 0, 0);
}

int _unlink(char const* pathname)
{
    return syscall(SYS_Unlink, (uint32_t) pathname, 0, 0, 0, 0, 0);
}

void _kill(int pid, int sig)
{
    syscall(SYS_Kill, (uint32_t) pid, (uint32_t) sig, 0, 0, 0, 0);
}

int _getpid(void)
{
    return syscall(SYS_GetPid, 0, 0, 0, 0, 0, 0);
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

pid_t _fork(void)
{
    return syscall(SYS_Fork, 0, 0, 0, 0, 0, 0);
}

int _execve(const char *pathname, char *const _Nullable argv[], char *const _Nullable envp[])
{
    return syscall(SYS_Execve, (uintptr_t) pathname, (uintptr_t) argv, (uintptr_t) envp, 0, 0, 0);
}

pid_t waitpid(pid_t pid, int *wstatus, int options)
{
    return syscall(SYS_WaitPid, (uintptr_t) pid, (uintptr_t) wstatus, (uintptr_t) options, 0, 0, 0);
}

pid_t _wait(int *wstatus)
{
    return waitpid(-1, wstatus, 0);
}

int mkdir(char const* pathname, mode_t mode)
{
    return syscall(SYS_MakeDirectory, (uint32_t) pathname, (uint32_t) mode, 0, 0, 0, 0);
}

DIR *opendir(const char *name)
{
    int fd = 0;
    DIR *dir = NULL;
    struct dirent *dirent = NULL;
    
    dir = malloc(sizeof(DIR));
    dirent = malloc(sizeof(struct dirent));
    if (dir == NULL || dirent == NULL)
        goto failed;
    
    fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        goto failed;

    dir->fd = fd;
    dir->ent = dirent;

    return dir;

failed:
    free(dirent);
    free(dir);
    if (fd < 0)
        close(fd);
    return NULL;
}

struct dirent *readdir(DIR *dirp)
{
    int rc;
    if (dirp == NULL)
        return NULL;
    
    rc = read(dirp->fd, dirp->ent, sizeof(struct dirent));
    if (rc == sizeof(struct dirent)) {
        return dirp->ent;
    } else {
        return NULL;
    }
}

int closedir(DIR *dirp)
{
    if (dirp == NULL)
        return 0;
    
    close(dirp->fd);
    free(dirp->ent);
    free(dirp);
    return 0;
}
