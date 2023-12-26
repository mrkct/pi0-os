#pragma once

#include <api/syscalls.h>
#include <kernel/error.h>
#include <kernel/filesystem/filesystem.h>
#include <kernel/interrupt.h>
#include <kernel/memory/vm.h>

namespace kernel {

enum class TaskState {
    Running,
    Suspended,
    Zombie,
};

typedef void (*OnTaskExitHandler)(void *arg);

struct OnTaskExitHandlerListItem {
    OnTaskExitHandler callback;
    void *arg;
    struct OnTaskExitHandlerListItem *next;
};

struct Task {
    int exit_code;
    TaskState task_state;
    AddressSpace address_space;
    SuspendedTaskState state;
    char name[32];
    PID pid;
    uint32_t time_slice;
    struct {
        size_t len, allocated;
        int next_fd;
        struct {
            uint32_t fd;
            File* file;
        }* entries;
    } open_files;
    Task* next_to_run;

    OnTaskExitHandlerListItem *on_task_exit_list;
};

void scheduler_init();

[[noreturn]] void scheduler_begin();

Task* scheduler_current_task();

Error task_create_kernel_thread(PID&, char const* name, int argc, const char *argv[], void (*entry)());

Error task_load_user_elf(
    PID&,
    char const* name,
    int argc,
    const char *argv[],
    uint8_t const* elf_binary,
    size_t elf_binary_size
);

Error task_load_user_elf_from_path(
    PID&,
    char const* pathname,
    int argc,
    const char *argv[]
);

void scheduler_step(SuspendedTaskState*);

void change_task_state(Task*, TaskState);

Error task_open_file(Task*, char const*, uint32_t flags, uint32_t&);

Error task_close_file(Task*, uint32_t);

Error task_get_open_file(Task*, uint32_t, File*&);

Task *find_task_by_pid(PID);

Error task_add_on_exit_handler(Task*, OnTaskExitHandler handler, void *arg);

}
