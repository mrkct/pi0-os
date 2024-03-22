#ifndef API_PROCESS_H
#define API_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "syscalls.h"


#ifdef __cplusplus
namespace api {
#endif

typedef uint32_t PID;

typedef struct ProcessInfo {
    PID pid;
    char name[32];
} ProcessInfo;

typedef struct SpawnProcessConfig {
    uint32_t flags;
    const char **args;
    uint32_t args_len;
    int32_t *descriptors;
    uint32_t descriptors_len;
} SpawnProcessConfig;

static inline int spawn_process(const char *path, SpawnProcessConfig *cfg, PID *out_pid)
{
    return syscall(SYS_SpawnProcess, out_pid, (uint32_t) path, (uint32_t) cfg, 0, 0, 0);
}

static inline int await_process(PID pid)
{
    return syscall(SYS_AwaitProcess, NULL, (uint32_t) pid, 0, 0, 0, 0);
}

static inline int get_process_info(ProcessInfo *out_info)
{
    return syscall(SYS_GetProcessInfo, NULL, (uint32_t) out_info, 0, 0, 0, 0);
}

static inline int create_pipe(int32_t out_fds[2])
{
    return syscall(SYS_CreatePipe, NULL, (uint32_t) out_fds, 0, 0, 0, 0);
}

#ifdef __cplusplus
}
#endif

#endif