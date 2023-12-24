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
            int fd;
            File* file;
        }* entries;
    } open_files;
    Task* next_to_run;
};

void scheduler_init();

[[noreturn]] void scheduler_begin();

Task* scheduler_current_task();

Error task_create_kernel_thread(char const* name, void (*entry)());

Error task_load_user_elf(char const* name, uint8_t const* elf_binary, size_t elf_binary_size);

Error task_load_user_elf_from_path(char const* pathname);

void scheduler_step(SuspendedTaskState*);

void change_task_state(Task*, TaskState);

Error task_open_file(Task*, char const*, uint32_t flags, int&);

Error task_close_file(Task*, int);

Error task_get_open_file(Task*, int, File*&);

}
