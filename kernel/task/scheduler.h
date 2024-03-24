#pragma once

#include <api/syscalls.h>
#include <kernel/error.h>
#include <kernel/vfs/vfs.h>
#include <kernel/interrupt.h>
#include <kernel/memory/vm.h>

namespace kernel {

static constexpr int MAX_OPEN_FILES_PER_TASK = 32;

enum class TaskState {
    Running,
    Suspended,
    Zombie,
};

typedef void (*OnTaskExitHandler)(void* arg);

struct OnTaskExitHandlerListItem {
    OnTaskExitHandler callback;
    void* arg;
    struct OnTaskExitHandlerListItem* next;
};

struct TaskFileCustody {
    FileCustody custody;
    bool wakeup_on_update;
};

struct Task {
    int exit_code;
    TaskState task_state;
    AddressSpace address_space;
    SuspendedTaskState state;
    char name[32];
    api::PID pid;
    uint32_t time_slice;
    TaskFileCustody open_files[MAX_OPEN_FILES_PER_TASK];
    Task* next_to_run;

    OnTaskExitHandlerListItem* on_task_exit_list;
};

void scheduler_init();

[[noreturn]] void scheduler_begin();

Task* scheduler_current_task();

Error task_create_kernel_thread(api::PID&, char const* name, int argc, char const* argv[], void (*entry)());

Error task_load_user_elf(
    api::PID&,
    char const* name,
    int argc,
    char const* argv[],
    uint8_t const* elf_binary,
    size_t elf_binary_size);

Error task_load_user_elf_from_path(
    api::PID&,
    char const* pathname,
    int argc,
    char const* const argv[]);

void scheduler_step(SuspendedTaskState*);

void change_task_state(Task*, TaskState);

int32_t task_find_free_file_descriptor(Task*);

Error task_get_file_by_descriptor(Task*, int32_t fd, FileCustody*&);

Error task_reserve_n_file_descriptors(Task*, uint32_t n, int32_t out_fds[]);

Error task_set_file_descriptor(Task*, int32_t fd, FileCustody);

Error task_drop_file_descriptor(Task *task, int32_t fd);

void task_inherit_file_descriptors(Task *parent, Task *child);

Error task_wakeup_on_fd_update(Task *task, int32_t fd);

Task* find_task_by_pid(api::PID);

Error task_add_on_exit_handler(Task*, OnTaskExitHandler handler, void* arg);

void scheduler_notify_file_update(File *file);

}
