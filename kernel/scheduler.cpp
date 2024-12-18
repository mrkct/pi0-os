#include <unistd.h>
#include <kernel/arch/arch.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/vm.h>
#include <kernel/timer.h>
#include <kernel/locking/irqlock.h>
#include <kernel/locking/mutex.h>
#include <kernel/lib/arrayutils.h>
#include <kernel/lib/intrusivelinkedlist.h>
#include <kernel/task/elfloader.h>

#include <api/arm/crt0util.h>

#include "scheduler.h"

#define LOG_ENABLED
#define LOG_TAG "SCHED"
#include <kernel/log.h>


int s_next_available_pid = 0;

static Thread *s_current_thread = nullptr;
static Thread *s_all_threads[64] = {0};
static size_t s_all_threads_len = 0;
static ContextSwitchFrame *s_scheduler_ctx = nullptr;


static void free_process(Process *process);
static void free_thread(Thread *thread);

static void free_process(Process *process)
{
    auto lock = irq_lock();
    while (process->threads.count > 0)
        free_thread(process->threads.data[0]);
    

    release(lock);
}

static void free_thread(Thread *thread)
{
    auto lock = irq_lock();
    Process *parent = thread->process;
    kfree(thread);

    array_swap_remove(parent->threads.data, parent->threads.count, thread);
    parent->threads.count--;

    if (parent->threads.count == 0)
        kfree(parent);

    release(lock);
}

static void *alloc_kernel_stack()
{
    PhysicalPage *kernel_stack_page;
    if (!physical_page_alloc(PageOrder::_4KB, kernel_stack_page).is_success())
        return nullptr;
    
    uint8_t *addr = reinterpret_cast<uint8_t*>(phys2virt(page2addr(kernel_stack_page)));
    addr += _4KB - 8;
    return addr;
}

static void *alloc_thread_user_stack(AddressSpace *address_space, int tid)
{
    static constexpr size_t STARTING_SIZE = _4KB * 4;
    static constexpr size_t MAX_SIZE = 2 * _1MB;
    uintptr_t startaddr = areas::KERNEL_VIRT_START_ADDR - (MAX_SIZE * tid);

    PhysicalPage *pages[STARTING_SIZE / _4KB] = {0};
    for (size_t i = 0; i < array_size(pages); i++) {
        if (!physical_page_alloc(PageOrder::_4KB, pages[i]).is_success()) {

            for (size_t j = 0; j < i; j++) {
                physical_page_free(pages[j], PageOrder::_4KB);
            }
            return nullptr;

        }
    }

    auto lock = irq_lock();
    for (size_t i = 0; i < array_size(pages); i++) {
        uintptr_t virt = startaddr - ((i + 1) * _4KB);
        MUST(vm_map(*address_space, pages[i], virt, PageAccessPermissions::UserFullAccess));
    }

    release(lock);

    return reinterpret_cast<void*>(startaddr - 8);
}

/**
 * Pushes on the argument stack an array of string, including an extra '\0' terminator,
 * plus also an array of pointers to the strings on the stack.
 * In the last argument, this function stores the stack address at which the array of
 * strings was placed.
 * 
 * This function moves the stack pointer and also aligns it to the ARCH_STACK_ALIGNMENT
 * boundary.
 */
static uint8_t *push_string_array_to_stack(
    uint8_t *userstack,
    char const* const array[],
    uintptr_t *out_array_start_addr,
    size_t *out_array_size
)
{
    const char **user_array = nullptr;
    const char *next_string = (const char *) userstack;
    size_t array_size = 0;

    // First we copy all the strings, one after the other
    for (size_t i = 0; array[i] != nullptr; i++, array_size++) {
        size_t len = strlen(array[i]) + 1;
        userstack -= len;
        memcpy(userstack, array[i], len);
    }
    userstack -= 1;
    *userstack = '\0';
    userstack = (uint8_t*) round_down((uintptr_t) userstack, ARCH_STACK_ALIGNMENT);

    // Then we iterate back to build the array
    userstack -= sizeof(uintptr_t) * (array_size + 1);
    user_array = (const char**) userstack;
    for (size_t i = 0; i < array_size + 1; i++) {
        user_array[i] = next_string;
        next_string += strlen(next_string) + 1;
    }

    *out_array_start_addr = (uintptr_t) userstack;
    *out_array_size = array_size;
    
    userstack = (uint8_t*) round_down((uintptr_t) userstack, ARCH_STACK_ALIGNMENT);
    return userstack;
}

static uint8_t *push_process_args(uint8_t *userstack, char *const argv[], char *const envp[])
{
    size_t argc, envc = 0;
    uintptr_t user_argv, user_envp;

    userstack = push_string_array_to_stack(userstack, argv, &user_argv, &argc);
    userstack = push_string_array_to_stack(userstack, envp, &user_envp, &envc);

    userstack -= sizeof(ArmCrt0InitialStackState);
    ArmCrt0InitialStackState *state = (ArmCrt0InitialStackState*) userstack;
    *state = (ArmCrt0InitialStackState) {
        .argc = argc,
        .argv = (char**) user_argv,
        .envc = envc,
        .envp = (char**) user_envp,
    };

    userstack = (uint8_t*) round_down((uintptr_t) userstack, ARCH_STACK_ALIGNMENT);

    return userstack;
}

static void register_thread(Thread *thread)
{
    auto lock = irq_lock();
    kassert(s_all_threads_len < array_size(s_all_threads));
    s_all_threads[s_all_threads_len++] = thread;
    release(lock);
}

Process *alloc_process(const char *name, void (*entrypoint)(), bool privileged)
{
    Process *new_process = (Process*) malloc(sizeof(Process));
    Thread **threads_array = (Thread**) malloc(sizeof(Thread*));
    Thread *first_thread = (Thread*) malloc(sizeof(Thread));
    void *user_stack = nullptr;

    if (new_process == nullptr || threads_array == nullptr || first_thread == nullptr) {
        LOGE("Failed to alloc memory for new process");
        goto cleanup;
    }

    new_process->next_available_tid = 0;
    new_process->exit_code = 0;
    new_process->pid = s_next_available_pid++;
    strcpy(const_cast<char*>(new_process->name), name);
    
    if (!vm_create_address_space(new_process->address_space).is_success()) {
        LOGE("Failed to create address space for new process %s\n", name);
        goto cleanup;
    }

    new_process->threads.data = threads_array;
    new_process->threads.allocated = 1;
    new_process->threads.count = 1;
    new_process->threads.data[0] = first_thread;
    for (size_t i = 0; i < array_size(new_process->openfiles); i++)
        new_process->openfiles[i] = nullptr;

    first_thread->tid = new_process->next_available_tid++;
    first_thread->process = new_process;
    first_thread->kernel_stack_ptr = alloc_kernel_stack();
    if (first_thread->kernel_stack_ptr == nullptr) {
        LOGE("Failed to allocate kernel stack for first thread of new process %s\n", name);
        goto cleanup;
    }
    first_thread->state = ThreadState::Suspended;
    user_stack = alloc_thread_user_stack(&new_process->address_space, 0);
    if (user_stack == nullptr) {
        LOGE("Failed to allocate user stack for first thread of new process %s\n", name);
        goto cleanup;
    }

    arch_create_initial_kernel_stack(
        &first_thread->kernel_stack_ptr,
        &first_thread->iframe,
        reinterpret_cast<uintptr_t>(user_stack),
        reinterpret_cast<uintptr_t>(entrypoint),
        privileged);

    register_thread(first_thread);
    return new_process;

cleanup:
    if (new_process) {
        vm_free(new_process->address_space);
        if (new_process->threads.data[0]->kernel_stack_ptr) {
            // TODO: Free the kernel stack memory
        }
    }
    free(new_process);
    free(threads_array);
    free(first_thread);


    return nullptr;
}

Thread  *cpu_current_thread()    { return s_current_thread; }
Process *cpu_current_process()   { return cpu_current_thread()->process; }

void create_first_process(void (*entrypoint)(void))
{
    int rc;
    FileCustody *temp;
    Process *stage2 = alloc_process("kernel", entrypoint, true);
    kassert(stage2 != nullptr);
    Thread *thread = stage2->threads.data[0];

    rc = vfs_open("/dev/kernel_log", O_RDONLY, &temp);
    kassert(rc == 0);
    stage2->openfiles[STDIN_FILENO] = temp;
    
    rc = vfs_open("/dev/kernel_log", O_WRONLY, &temp);
    kassert(rc == 0);
    stage2->openfiles[STDOUT_FILENO] = temp;

    rc = vfs_open("/dev/kernel_log", O_WRONLY, &temp);
    kassert(rc == 0);
    stage2->openfiles[STDERR_FILENO] = temp;

    thread->state = ThreadState::Runnable;
    s_current_thread = thread;
}

void scheduler_start()
{
    // timer_install_scheduler_callback(5, scheduler_step);

    while (true) {
        for (size_t i = 0; i < array_size(s_all_threads); i++) {
            Thread *thread = s_all_threads[i];
            if (thread == nullptr)
                continue;
            
            if (thread->state == ThreadState::Zombie) {
                free_thread(thread);
                s_all_threads[i] = nullptr;
                continue;
            } else if (thread->state == ThreadState::Suspended) {
                continue;
            }

            kassert(thread->state == ThreadState::Runnable);
            LOGD("Context switching to %s[%d/%d]", thread->process->name, thread->process->pid, thread->tid);

            // This should be protected someway if we want to support multicore
            // otherwise 2 cores could end up scheduling the same thread using the same kernel stack
            vm_switch_address_space(thread->process->address_space);
            s_current_thread = thread;
            arch_context_switch(&s_scheduler_ctx, reinterpret_cast<ContextSwitchFrame*>(thread->kernel_stack_ptr));
        }

        cpu_relax();
    }
}

int sys$exit(int exit_code)
{
    auto *current_process = cpu_current_process();
    auto *current_thread = cpu_current_thread();

    LOGI("Exiting %s[%d/%d] with exit code %d\n", current_process->name, current_process->pid, current_thread->tid, exit_code);
    current_thread->state = ThreadState::Zombie;
    current_process->exit_code = exit_code;
    sys$yield();
    panic("managed to return from sys$exit. this should never be reached");
    return 0;
}

int sys$yield()
{
    arch_context_switch(reinterpret_cast<ContextSwitchFrame**>(&s_current_thread->kernel_stack_ptr), s_scheduler_ctx);
    return 0;
}

int sys$fork()
{
    int rc = 0;
    FileCustody *custody = nullptr;
    Process *forked = nullptr;
    Thread *forked_thread = nullptr;
    auto *current_process = cpu_current_process();
    auto *current_thread = cpu_current_thread();

    LOGI("Forking %s[%d/%d]", current_process->name, current_process->pid, current_thread->tid);

    // entrypoint and priviledged don't matter, we're going to copy the other process state anyway
    forked = alloc_process(current_process->name, NULL, false);
    if (forked == nullptr) {
        LOGE("Failed to allocate memory to fork process");
        rc = -ENOMEM;
        goto failed;
    }
    
    for (size_t i = 0; i < array_size(current_process->openfiles); i++) {
        custody = current_process->openfiles[i];
        if (custody == nullptr)
            continue;

        forked->openfiles[i] = vfs_duplicate(custody);
        if (forked->openfiles[i] == nullptr) {
            LOGE("Failed to duplicate open file");
            rc = -ENOMEM;
            goto failed;
        }
    }

    forked_thread = forked->threads.data[0];
    if (Error err = vm_fork(current_process->address_space, forked->address_space); !err.is_success()) {
        LOGE("Failed to fork address space");
        rc = -ENOMEM;
        goto failed;
    }

    *(forked_thread->iframe) = *(current_thread->iframe);
    forked_thread->iframe->set_syscall_return_value(0);

    forked_thread->state = ThreadState::Runnable;
    return forked->pid;

failed:
    free_process(forked);
    return rc;
}

int sys$execve(const char *path, char *const argv[], char *const envp[])
{
    (void)argv;
    (void)envp;

    int rc = 0;
    uintptr_t entrypoint;
    uint8_t *userstack = nullptr;
    auto *current_process = cpu_current_process();
    auto *current_thread = cpu_current_thread();
    AddressSpace old_as, new_as;

    LOGI("execve %s[%d/%d](%s)", current_process->name, current_process->pid, current_thread->tid, path);
    
    if (!vm_create_address_space(new_as).is_success()) {
        LOGE("Failed to create address space for new process");
        rc = -ENOMEM;
        goto cleanup;
    }

    rc = elf_load_into_address_space(path, &entrypoint, new_as);
    if (rc != 0) {
        LOGE("Failed to load ELF file '%s', rc=%s(%d)\n", path, strerror(rc), rc);
        goto cleanup;
    }
    LOGD("Successsfully loaded ELF file '%s', entrypoint: %p", path, entrypoint);

    userstack = (uint8_t*) alloc_thread_user_stack(&new_as, 0);
    if (userstack == nullptr) {
        LOGE("Failed to allocate user stack for new process");
        rc = -ENOMEM;
        goto cleanup;
    }
    userstack = push_process_args(userstack, argv, envp);

    // NOTE: We're going to use the same kernel stack, but we
    // don't need to do anything because the used page is used
    // through the physical memory hole, therefore when vm_map
    // will be called it will stay alive

    // TODO: Destroy all threads except 1
    if (current_process->threads.count > 1) {
        panic("execve for multiple threads not implemented");
    }

    old_as = current_process->address_space;
    current_process->address_space = new_as;
    vm_switch_address_space(current_process->address_space);
    vm_free(old_as);

    kassert((uintptr_t) userstack % ARCH_STACK_ALIGNMENT == 0);
    current_thread->iframe->set_thread_start_values(entrypoint, (uintptr_t) userstack);

    return 0;

cleanup:
    vm_free(new_as);
    return rc;
}

int sys$open(const char *path, int flags, int mode)
{
    (void)mode;

    int rc = 0;
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    int fd = -1;
    for (unsigned i = 0; i < array_size(current_process->openfiles); i++) {
        if (current_process->openfiles[i] == nullptr) {
            fd = (int) i;
            break;
        }
    }
    if (fd == -1) {
        LOGE("Process %s[%d] failed to open '%s', no free file descriptors", current_process->name, current_process->pid, path);
        return -ENFILE;
    }

    rc = vfs_open(path, flags, &file);
    if (rc != 0) {
        LOGE("Process %s[%d] failed to open '%s', rc=%d", current_process->name, current_process->pid, path, rc);
        return rc;
    }
    current_process->openfiles[fd] = file;
    return fd;
}

int sys$read(int fd, void *buf, size_t count)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -EBADF;

    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -EBADF;
    
    return vfs_read(file, (uint8_t*) buf, count);
}

int sys$write(int fd, const void *buf, size_t count)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -EBADF;

    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -EBADF;
    
    return vfs_write(file, (const uint8_t*) buf, count);
}

int sys$close(int fd)
{
    int rc;
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -EBADF;
    
    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -EBADF;
    rc = vfs_close(file);
    current_process->openfiles[fd] = nullptr;

    return rc;
}

int sys$millisleep(int ms)
{
    auto *current_thread = cpu_current_thread();
    Mutex lock;

    LOGI("%s[%d] going to sleep for %d ms", current_thread->process->name, current_thread->tid, ms);
    if (ms < 0)
        return -EINVAL;
    else if (ms == 0)
        return 0;

    mutex_init(lock, MutexInitialState::Locked);
    timer_exec_once(ms, [](void *lock) { 
        LOGI("Sleep timer expired");
        mutex_release(*(Mutex*) lock);
    }, &lock);
    mutex_take(lock);
    LOGI("%s[%d] woke up", current_thread->process->name, current_thread->tid);

    return 0;
}
