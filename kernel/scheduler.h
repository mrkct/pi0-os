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

    struct ProcessExitListener {
        INTRUSIVE_LINKED_LIST_HEADER(ProcessExitListener);

        void *arg;
        void (*callback)(Process *process, void *arg);
    };

    int next_available_tid;
    int pid;
    int exit_code;
    char name[64];
    AddressSpace address_space;
    FileCustody *openfiles[16];
    char *working_directory;
    IntrusiveLinkedList<ProcessExitListener> process_exit_listeners;

    struct {
        Thread **data;
        size_t allocated;
        size_t count;
    } threads;

    bool is_zombie() const {
        for (size_t i = 0; i < this->threads.count; i++) {
            if (this->threads.data[i]->state != ThreadState::Zombie)
                return false;
        }

        return true;
    }
};


void create_first_process(void (*entrypoint)(void));

void scheduler_start();

bool scheduler_has_started();
Process *cpu_current_process();
Thread *cpu_current_thread();

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

int sys$getticks();

int sys$getpid();

int sys$create_pipe(int *write_fd, int *read_fd);

int sys$movefd(int fd, int new_fd);

int sys$poll(api::PollFd *fds, int nfds, int timeout);

int sys$mmap(int fd, uintptr_t vaddr, uint32_t length, uint32_t flags);

int sys$istty(int fd);

int sys$dup2(int oldfd, int newfd);

int sys$waitexit(int pid);

int sys$setcwd(const char *path);

int sys$getcwd(char *buf, size_t buflen);
