#pragma once

#include <kernel/base.h>
#include <kernel/arch/arch.h>
#include <kernel/memory/vm.h>
#include <kernel/lib/intrusivelinkedlist.h>
#include <kernel/vfs/vfs.h>


enum class ThreadState {
    Runnable,
    Suspended,
    Zombie,
};

struct Process;

struct Thread {
    INTRUSIVE_LINKED_LIST_HEADER(Thread);    

    int tid;
    Process *process;
    InterruptFrame *iframe;
    void *kernel_stack_ptr;
    ThreadState state;
};

struct Process {
    INTRUSIVE_LINKED_LIST_HEADER(Process);

    int next_available_tid;
    int pid;
    int exit_code;
    char name[64];
    AddressSpace address_space;
    FileCustody *openfiles[16];

    struct {
        Thread **data;
        size_t allocated;
        size_t count;
    } threads;
};


void create_first_process(void (*entrypoint)(void));

void scheduler_start();

Process *cpu_current_process();
Thread *cpu_current_thread();

/**
 * \brief Allocates a new process
 * 
 * Allocates a new process with the given name and 1 thread
 * starting at \ref entrypoint
 * 
 * The initial thread will have its \ref Thread::state set
 * to \ref ThreadState::Suspended. This is to allow extra
 * initialization to be done before starting the thread.
 * 
 * Once you have completed their initialization you should change their
 * state to start them
*/
Process *alloc_process(const char *name, void (*entrypoint)(), bool privileged);

int sys$exit(int exit_code);

int sys$yield();

int sys$fork();

int sys$execve(const char *path, char *const argv[], char *const envp[]);

int sys$open(const char *path, int flags, int mode);

int sys$read(int fd, void *buf, size_t count);

int sys$write(int fd, const void *buf, size_t count);

int sys$close(int fd);

int sys$ioctl(int fd, uint32_t ioctl, void *argp);

int sys$fstat(int fd, api::Stat *stat);

int sys$seek(int fd, int offset, int whence, uint64_t *out_new_offset);

int sys$millisleep(int ms);

int sys$getpid();

int sys$create_pipe(int *write_fd, int *read_fd);

int sys$movefd(int fd, int new_fd);

int sys$poll(api::PollFd *fds, int nfds, int timeout);

int sys$mmap(int fd, uintptr_t vaddr, uint32_t length, uint32_t flags);
