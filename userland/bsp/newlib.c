#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <api/syscalls.h>


#define SET_ERRNO_AND_RETURN(expr)      \
    do {                                \
        int rc = (expr);                \
        if (-4096 < rc && rc < 0) {     \
            errno = -rc;                \
            return -1;                  \
        } else {                        \
            errno = 0;                  \
            return rc;                  \
        }                               \
    } while (0)


void _exit(int status)
{
    sys_exit(status);
    while(1);
}

static uint8_t s_heap[16 * 1024 * 1024];
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

static void unix_stat_to_posix_stat(struct Stat *unix_stat, struct stat *posix_stat)
{
    posix_stat->st_dev = unix_stat->st_dev;
    posix_stat->st_ino = unix_stat->st_ino;
    posix_stat->st_mode = unix_stat->st_mode;
    posix_stat->st_nlink = unix_stat->st_nlink;
    posix_stat->st_uid = unix_stat->st_uid;
    posix_stat->st_gid = unix_stat->st_gid;
    posix_stat->st_rdev = unix_stat->st_rdev;
    posix_stat->st_size = unix_stat->st_size;
    posix_stat->st_blksize = unix_stat->st_blksize;
    posix_stat->st_blocks = unix_stat->st_blocks;
    posix_stat->st_atime = unix_stat->atim.seconds;
    posix_stat->st_mtime = unix_stat->mtim.seconds;
    posix_stat->st_ctime = unix_stat->ctim.seconds;
}

int _fstat(int file, struct stat* st)
{
    struct Stat tempstat;
    int rc = sys_fstat(file, &tempstat);
    if (rc >= 0) {
        unix_stat_to_posix_stat(&tempstat, st);
    }
    return rc;
}

int _isatty(int file)
{
    return sys_isatty(file);
}

off_t _lseek(int file, int ptr, int dir)
{
    int rc;
    uint64_t offset = 0;
    rc = sys_seek(file, ptr, dir, &offset);
    if (rc < 0)
        return -1;
    return (off_t) offset;
}

int _link(char const* oldpath, char const* newpath)
{
    SET_ERRNO_AND_RETURN(sys_link(oldpath, newpath));
}

int _unlink(char const* pathname)
{
    SET_ERRNO_AND_RETURN(sys_unlink(pathname));
}

void _kill(int pid, int sig)
{
    sys_kill(pid, sig);
}

int _getpid(void)
{
    return sys_getpid();
}

int _open(char const* pathname, int flags, int mode)
{
    SET_ERRNO_AND_RETURN(sys_open(pathname, flags, mode));
}

int _close(int file)
{
    SET_ERRNO_AND_RETURN(sys_close(file));
}

int _write(int file, char* ptr, int len)
{
    SET_ERRNO_AND_RETURN(sys_write(file, ptr, len));
}

int _read(int file, char* ptr, int len)
{
    SET_ERRNO_AND_RETURN(sys_read(file, ptr, len));
}

pid_t _fork(void)
{
    SET_ERRNO_AND_RETURN(sys_fork());
}

int _execve(const char *pathname, char *const _Nullable argv[], char *const _Nullable envp[])
{
    SET_ERRNO_AND_RETURN(sys_execve(pathname, (const char * const *) argv, (const char * const *) envp));
}

pid_t waitpid(pid_t pid, int *wstatus, int options)
{
    SET_ERRNO_AND_RETURN(sys_waitpid(pid, wstatus, options));
}

pid_t _wait(int *wstatus)
{
    SET_ERRNO_AND_RETURN(waitpid(-1, wstatus, 0));
}

int _gettimeofday (struct timeval *tv, void *)
{
    *tv = (struct timeval) {
        .tv_sec = 1737399329,
        .tv_usec = 0
    };
    return 0;
}

int mkdir(char const* pathname, mode_t mode)
{
    SET_ERRNO_AND_RETURN(sys_mkdir(pathname, mode));
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
    
    fd = open(name, OF_RDONLY | OF_DIRECTORY);
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

int ioctl (int fd, unsigned long int request, void *argp)
{
    SET_ERRNO_AND_RETURN(sys_ioctl(fd, request, argp));
}

int chdir(const char *path)
{
    SET_ERRNO_AND_RETURN(sys_chdir(path));
}

char *getcwd(char *buf, size_t size)
{
    int rc = sys_getcwd(buf, size);
    if (rc < 0) {
        errno = -rc;
        return NULL;
    } else {
        return buf;
    }
}
