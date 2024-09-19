#pragma once

#include <kernel/base.h>
#include <kernel/arch/arch.h>
#include <kernel/memory/vm.h>
#include <kernel/lib/intrusivelinkedlist.h>


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
    const char name[64];
    AddressSpace address_space;
    
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

int sys$yield();

int sys$fork();
