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
#define LOG_CTX_SWITCH 0
#define LOG_TAG "SCHED"
#include <kernel/log.h>


int s_next_available_pid = 0;
static bool g_scheduler_has_started = false;
static Thread *s_current_thread = nullptr;
static Thread *s_all_threads[64] = {0};
static size_t s_all_threads_len = 0;
static ContextSwitchFrame *s_scheduler_ctx = nullptr;


static void free_process(Process *process);
static void free_thread(Thread *thread);

static void free_process(Process *process)
{
    auto lock = irq_lock();
    
    LOGD("Freeing process %s[%d]", process->name, process->pid);
    for (size_t i = 0; i < process->threads.count; i++)
        free_thread(process->threads.data[0]);
    free(process->threads.data);
    LOGD("All threads freed");
    for (size_t i = 0; i < array_size(process->openfiles); i++) {
        FileCustody *custody = process->openfiles[i];
        if (custody == nullptr)
            continue;

        LOGD("Closing file %d", i);
        vfs_close(custody);
    }
    LOGD("All files closed, freeing address space");
    vm_free(process->address_space);
    LOGD("Done, freeing the process structure");
    kfree(process);

    release(lock);
}

static void free_thread(Thread *thread)
{
    auto lock = irq_lock();
    Process *parent = thread->process;
    kfree(thread);

    LOGD("Freeing thread %s[%d/%d]", parent->name, parent->pid, thread->tid);
    array_swap_remove(parent->threads.data, parent->threads.count, thread);
    parent->threads.count--;

    // Remove from the scheduling queue (if it's there)
    unsigned idx = array_find(s_all_threads, s_all_threads_len, thread);
    if (idx != s_all_threads_len) {
        s_all_threads[idx] = nullptr;
    }

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

static void free_kernel_stack(void *kernel_stack_ptr)
{
    auto addr_page = round_down(reinterpret_cast<uintptr_t>(kernel_stack_ptr), _4KB);
    PhysicalPage *page = addr2page(virt2phys(addr_page));
    kassert(page != nullptr);
    physical_page_free(page, PageOrder::_4KB);
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
 * Clones a NULL-terminated array of NULL-terminated strings coming from userspace
 * into a heap-allocated array of heap-allocated strings.
 * 
 */
static int clone_user_array_of_strings(char const *const user_array[], char ***out_array, size_t *out_array_size)
{
    constexpr size_t MAX_STRING_SIZE = 256;
    constexpr size_t MAX_ARRAY_SIZE = 128;

    int rc = 0;
    char **array = nullptr;
    size_t array_size = 0;

    while (user_array[array_size] != nullptr && array_size < MAX_ARRAY_SIZE)
        array_size++;
    if (array_size == MAX_ARRAY_SIZE) {
        LOGE("Too many string arguments");
        return -ERR_2BIG;
    }

    array = (char**) malloc(array_size * sizeof(char*));
    if (array == nullptr) {
        LOGE("Failed to allocate array of strings");
        rc = -ERR_NOMEM;
        goto failed;
    }

    for (size_t i = 0; i < array_size; i++) {
        size_t len = strnlen(user_array[i], MAX_STRING_SIZE);
        if (len == MAX_STRING_SIZE) {
            LOGE("String too long");
            rc = -ERR_2BIG;
            goto failed;
        }

        array[i] = strdup(user_array[i]);
        if (array[i] == nullptr) {
            LOGE("Failed to allocate string");
            rc = -ERR_NOMEM;
            goto failed;
        }
    }

    *out_array = array;
    *out_array_size = array_size;
    return 0;

failed:
    if (array != nullptr) {
        for (size_t i = 0; i < array_size; i++) {
            free(array[i]);
        }
        free(array);
    }

    return rc;
}

/**
 * Frees an array of strings allocated by clone_user_array_of_strings()
 */
static void free_array_of_strings(char *const *array, size_t array_size)
{
    if (array) {
        for (size_t i = 0; i < array_size; i++) {
            free((void*) array[i]);
        }
        free((void*) array);
    }
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
    size_t array_size,
    uintptr_t *out_array_start_addr    
)
{
    const char **user_array = nullptr;

    userstack -= sizeof(uintptr_t) * array_size;
    user_array = reinterpret_cast<const char**>(userstack);
    
    for (size_t i = 0; i < array_size; i++) {
        size_t len = strlen(array[i]) + 1;
        userstack -= len;
        memcpy(userstack, array[i], len);
        user_array[i] = reinterpret_cast<const char*>(userstack);
        LOGI("String '%s' stack-placed at %p", array[i], userstack);
    }

    *out_array_start_addr = reinterpret_cast<uintptr_t>(user_array);
    userstack = (uint8_t*) round_down((uintptr_t) userstack, ARCH_STACK_ALIGNMENT);

    return userstack;
}

static uint8_t *push_process_args(uint8_t *userstack,
    char *const argv[], size_t argc,
    char *const envp[], size_t envc
)
{
    uintptr_t user_argv, user_envp;

    userstack = push_string_array_to_stack(userstack, argv, argc, &user_argv);
    userstack = push_string_array_to_stack(userstack, envp, envc, &user_envp);

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

/**
 * \brief Allocates a new process with 1 thread
 * 
 * Allocates a new process with the given name and 1 thread
 * starting at \ref entrypoint
 * 
 * The initial thread will have its \ref Thread::state set
 * to \ref ThreadState::Suspended. This is to allow extra
 * initialization to be done before starting the thread.
 * 
 * The thread is not registed with the scheduler, therefore
 * once you're done with its initialization you should change
 * its state to \ref ThreadState::Running and call
 * \ref register_thread to make it schedulable.
*/
static Process *alloc_process(const char *name, void (*entrypoint)(), bool privileged)
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

    return new_process;

cleanup:
    if (new_process) {
        vm_free(new_process->address_space);
        if (new_process->threads.data[0]->kernel_stack_ptr) {
            free_kernel_stack(new_process->threads.data[0]->kernel_stack_ptr);
        }
    }
    free(new_process);
    free(threads_array);
    free(first_thread);


    return nullptr;
}

bool scheduler_has_started() { return g_scheduler_has_started; }
Thread  *cpu_current_thread()    { return s_current_thread; }
Process *cpu_current_process()   { return cpu_current_thread()->process; }

void create_first_process(void (*entrypoint)(void))
{
    int rc;
    FileCustody *temp;
    Process *stage2 = alloc_process("kernel", entrypoint, true);
    kassert(stage2 != nullptr);
    Thread *thread = stage2->threads.data[0];
    register_thread(thread);

    rc = vfs_open("/dev/kernel_log", OF_RDONLY, &temp);
    kassert(rc == 0);
    stage2->openfiles[STDIN_FILENO] = temp;
    
    rc = vfs_open("/dev/kernel_log", OF_WRONLY, &temp);
    kassert(rc == 0);
    stage2->openfiles[STDOUT_FILENO] = temp;

    rc = vfs_open("/dev/kernel_log", OF_WRONLY, &temp);
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
                LOGD("Thread %s[%d/%d] is a zombie, freeing it", thread->process->name, thread->process->pid, thread->tid);
                free_thread(thread);
                s_all_threads[i] = nullptr;
                continue;
            } else if (thread->state == ThreadState::Suspended) {
                continue;
            }

            kassert(thread->state == ThreadState::Runnable);
#if LOG_CTX_SWITCHES
            LOGD("Context switching to %s[%d/%d] (address table @ phys %p)", thread->process->name, thread->process->pid, thread->tid, page2addr(thread->process->address_space.ttbr0_page));
#endif
            // This should be protected someway if we want to support multicore
            // otherwise 2 cores could end up scheduling the same thread using the same kernel stack
            vm_switch_address_space(thread->process->address_space);
            s_current_thread = thread;
            arch_context_switch(&s_scheduler_ctx, reinterpret_cast<ContextSwitchFrame*>(thread->kernel_stack_ptr));
            g_scheduler_has_started = true;
        }

        cpu_relax();
    }
}

int sys$exit(int exit_code)
{
    auto *current_process = cpu_current_process();
    auto *current_thread = cpu_current_thread();

    LOGI("Exiting %s[%d/%d] with exit code %d", current_process->name, current_process->pid, current_thread->tid, exit_code);
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
        rc = -ERR_NOMEM;
        goto failed;
    }
    
    for (size_t i = 0; i < array_size(current_process->openfiles); i++) {
        custody = current_process->openfiles[i];
        if (custody == nullptr)
            continue;

        forked->openfiles[i] = vfs_duplicate(custody);
        if (forked->openfiles[i] == nullptr) {
            LOGE("Failed to duplicate open file");
            rc = -ERR_NOMEM;
            goto failed;
        }
    }

    forked_thread = forked->threads.data[0];
    if (Error err = vm_fork(current_process->address_space, forked->address_space); !err.is_success()) {
        LOGE("Failed to fork address space");
        rc = -ERR_NOMEM;
        goto failed;
    }

    *(forked_thread->iframe) = *(current_thread->iframe);
    forked_thread->iframe->set_syscall_return_value(0);

    forked_thread->state = ThreadState::Runnable;
    register_thread(forked_thread);
    return forked->pid;

failed:
    free_process(forked);
    return rc;
}

int sys$execve(const char *path, char *const user_argv[], char *const user_envp[])
{
    int rc = 0;
    uintptr_t entrypoint;
    uint8_t *userstack = nullptr;
    auto *current_process = cpu_current_process();
    auto *current_thread = cpu_current_thread();
    AddressSpace old_as, new_as;
    char **argv = nullptr;
    size_t argc = 0;
    char **envp = nullptr;
    size_t envc = 0;

    rc = clone_user_array_of_strings(user_argv, &argv, &argc);
    if (rc != 0) {
        LOGE("Failed to clone user argv");
        goto cleanup;
    }
    LOGD("Cloned argv (argc=%u)", argc);

    rc = clone_user_array_of_strings(user_envp, &envp, &envc);
    if (rc != 0) {
        LOGE("Failed to clone user envp");
        goto cleanup;
    }
    LOGD("Cloned envp (envc=%u)", envc);

    LOGI("execve %s[%d/%d](%s)", current_process->name, current_process->pid, current_thread->tid, path);
    
    if (!vm_create_address_space(new_as).is_success()) {
        LOGE("Failed to create address space for new process");
        rc = -ERR_NOMEM;
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
        rc = -ERR_NOMEM;
        goto cleanup;
    }
    // NOTE: We're still running with the old address space, pushing
    // to the userstack here won't put the data on the right stack!

    // NOTE: We're going to use the same kernel stack, but we
    // don't need to do anything because the used page is used
    // through the physical memory hole, therefore when vm_map
    // will be called it will stay alive

    // TODO: Destroy all threads except 1
    if (current_process->threads.count > 1) {
        panic("execve for multiple threads not implemented");
    }

    strncpy(current_process->name, path, sizeof(current_process->name) - 1);

    old_as = current_process->address_space;
    current_process->address_space = new_as;
    vm_switch_address_space(current_process->address_space);
    vm_free(old_as);

    userstack = push_process_args(userstack, argv, argc, envp, envc);
    kassert((uintptr_t) userstack % ARCH_STACK_ALIGNMENT == 0);
    current_thread->iframe->set_thread_start_values(entrypoint, (uintptr_t) userstack);

    return 0;

cleanup:
    free_array_of_strings(argv, argc);
    free_array_of_strings(envp, envc);
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
        return -ERR_NFILE;
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
        return -ERR_BADF;

    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -ERR_BADF;
    
    return vfs_read(file, (uint8_t*) buf, count);
}

int sys$write(int fd, const void *buf, size_t count)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -ERR_BADF;

    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -ERR_BADF;
    
    return vfs_write(file, (const uint8_t*) buf, count);
}

int sys$close(int fd)
{
    int rc;
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -ERR_BADF;
    
    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -ERR_BADF;
    rc = vfs_close(file);
    current_process->openfiles[fd] = nullptr;

    return rc;
}

int sys$ioctl(int fd, uint32_t ioctl, void *argp)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;
    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -ERR_BADF;

    file = current_process->openfiles[fd];
    return vfs_ioctl(file, ioctl, argp);
}

int sys$millisleep(int ms)
{
    auto *current_thread = cpu_current_thread();
    Mutex lock;

    LOGI("%s[%d] going to sleep for %d ms", current_thread->process->name, current_thread->tid, ms);
    if (ms < 0)
        return -ERR_INVAL;
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

int sys$fstat(int fd, api::Stat *stat)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -ERR_BADF;
    
    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -ERR_BADF;
    
    return vfs_fstat(file, stat);
}

int sys$seek(int fd, int offset, int whence, uint64_t *out_new_offset)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -ERR_BADF;
    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -ERR_BADF;

    ssize_t newoff = vfs_seek(file, whence, offset);
    if (newoff < 0)
        return (int) newoff;

    *out_new_offset = newoff;
    return 0;
}

int sys$getpid()
{
    return cpu_current_process()->pid;
}

int sys$create_pipe(int *write_fd, int *read_fd)
{
    int rc;
    auto *current_process = cpu_current_process();
    FileCustody *read_custody = nullptr;
    FileCustody *write_custody = nullptr;
    int read_fd_index = -1, write_fd_index = -1;

    rc = vfs_create_pipe(&write_custody, &read_custody);
    if (rc != 0) {
        LOGE("Failed to create pipe, rc=%d", rc);
        goto failed;
    }
    
    for (unsigned i = 0; i < array_size(current_process->openfiles); i++) {
        if (current_process->openfiles[i] == nullptr) {
            read_fd_index = i;
            break;
        }
    }
    if (read_fd_index == -1) {
        LOGE("Failed to create pipe, no free file descriptors");
        rc = -ERR_NFILE;
        goto failed;
    }

    current_process->openfiles[read_fd_index] = read_custody;
    for (unsigned i = 0; i < array_size(current_process->openfiles); i++) {
        if (current_process->openfiles[i] == nullptr) {
            write_fd_index = i;
            break;
        }
    }
    if (write_fd_index == -1) {
        LOGE("Failed to create pipe, no free file descriptors");
        rc = -ERR_NFILE;
        goto failed;
    }
    current_process->openfiles[write_fd_index] = write_custody;

    *read_fd = read_fd_index;
    *write_fd = write_fd_index;
    
    return 0;

failed:
    if (read_custody != nullptr)
        vfs_close(read_custody);
    if (write_custody != nullptr)
        vfs_close(write_custody);
    if (read_fd_index != -1)
        current_process->openfiles[read_fd_index] = nullptr;
    if (write_fd_index != -1)
        current_process->openfiles[write_fd_index] = nullptr;

    

    return rc;
}

int sys$movefd(int fd, int new_fd)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles))
        return -ERR_BADF;

    file = current_process->openfiles[fd];
    if (file == nullptr)
        return -ERR_BADF;

    if (new_fd < 0 || (unsigned) new_fd >= array_size(current_process->openfiles))
        return -ERR_BADF;
    
    if (current_process->openfiles[new_fd] != nullptr) {
        vfs_close(current_process->openfiles[new_fd]);
    }

    current_process->openfiles[new_fd] = file;
    current_process->openfiles[fd] = nullptr;

    return 0;
}

int sys$poll(api::PollFd *fds, int nfds, int timeout)
{
    int rc;
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;
    int available_fd = -1;
    uint64_t starttime = get_ticks_ms();

    do {
        for (int i = 0; i < nfds; i++) {
            if (fds[i].fd < 0)
                continue;

            if ((unsigned) fds[i].fd >= array_size(current_process->openfiles) || current_process->openfiles[fds[i].fd] == nullptr) {
                rc = -ERR_BADF;
                goto failed;
            }
            file = current_process->openfiles[fds[i].fd];
            fds[i].revents = 0;
            rc = vfs_poll(file, fds[i].events, &fds[i].revents);
            if (rc != 0) {
                LOGW("vfs_poll() failed for fd=%d, rc=%d", fds[i].fd, rc);
                goto failed;
            }

            if (fds[i].revents != 0) {
                available_fd = i;
                break;
            }
        }

        if (available_fd != -1)
            break;

        if (timeout > 0 && (int64_t) (get_ticks_ms() - starttime) > timeout) {
            rc = -ERR_TIMEDOUT;
            goto failed;
        }
        sys$yield();
    } while(true);

    rc = available_fd;
    return rc;

failed:
    return rc;
}

int sys$mmap(int fd, uintptr_t vaddr, uint32_t length, uint32_t flags)
{
    auto *current_process = cpu_current_process();
    auto *current_thread = cpu_current_thread();
    FileCustody *file = nullptr;

    if (!vm_addr_is_page_aligned(vaddr))
        return -ERR_INVAL;
    length = vm_align_up_to_page(length);

    if (fd < 0 || (unsigned) fd >= array_size(cpu_current_process()->openfiles) || cpu_current_process()->openfiles[fd] == nullptr)
        return -ERR_BADF;

    LOGI("%s[%d] mmap fd %d at %p", current_thread->process->name, current_thread->tid, fd, vaddr);
    file = current_process->openfiles[fd];
    return vfs_mmap(file, &current_process->address_space, vaddr, length, flags);
}

int sys$dup2(int fd, int new_fd)
{
    auto *current_process = cpu_current_process();
    FileCustody *file = nullptr;

    if (fd < 0 || (unsigned) fd >= array_size(current_process->openfiles) || current_process->openfiles[fd] == nullptr)
        return -ERR_BADF;
    if (new_fd < 0 || (unsigned) new_fd >= array_size(current_process->openfiles))
        return -ERR_BADF;

    if (current_process->openfiles[new_fd] != nullptr) {
        vfs_close(current_process->openfiles[new_fd]);
        current_process->openfiles[new_fd] = nullptr;
    }

    file = current_process->openfiles[fd];
    current_process->openfiles[new_fd] = vfs_duplicate(file);

    return 0;
}
