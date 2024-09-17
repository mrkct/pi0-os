#include <kernel/arch/arch.h>
#include <kernel/memory/areas.h>
#include <kernel/memory/vm.h>
#include <kernel/timer.h>
#include <kernel/locking/irqlock.h>
#include <kernel/lib/arrayutils.h>
#include <kernel/lib/intrusivelinkedlist.h>

#include "scheduler.h"


#define SCHEDULER_LOG_ENABLED
#ifdef SCHEDULER_LOG_ENABLED
#define LOG(fmt, ...) kprintf(fmt, __VA_ARGS__)
#else
#define LOG(fmt, ...) (fmt, __VA_ARGS__)
#endif


int s_next_available_pid = 0;

static Thread *s_current_thread = nullptr;
static Thread *s_all_threads[64] = {0};
static size_t s_all_threads_len = 0;
static ContextSwitchFrame *s_scheduler_ctx = nullptr;


static void free_thread(Thread *thread);


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

static void register_thread(Thread *thread)
{
    auto lock = irq_lock();
    kassert(s_all_threads_len < array_size(s_all_threads));
    s_all_threads[s_all_threads_len++] = thread;
    release(lock);
}

Process *alloc_process(const char *name, size_t thread_count)
{
    kassert(thread_count > 0);

    size_t required_mem = sizeof(Process) + // Process struct
        thread_count * sizeof(Thread*) +    // Array where the process stores pointers to all of its threads
        thread_count * sizeof(Thread);      // The threads themselves
    uint8_t *mem = reinterpret_cast<uint8_t*>(malloc(required_mem));
    if (mem == nullptr)
        return nullptr;

    Process *new_process = reinterpret_cast<Process*>(mem);
    mem += sizeof(Process);

    Thread **threads_array = reinterpret_cast<Thread**>(mem);
    mem += thread_count * sizeof(Thread*);

    Thread *new_threads = reinterpret_cast<Thread*>(mem);
    mem += thread_count * sizeof(Thread);


    new_process->next_available_tid = 0;
    new_process->exit_code = 0;
    new_process->pid = s_next_available_pid++;
    strcpy(const_cast<char*>(new_process->name), name);
    
    if (!vm_create_address_space(new_process->address_space).is_success()) {
        panic("cleanup not implemented yet");
        return nullptr;
    }

    new_process->threads.data = threads_array;
    new_process->threads.count = thread_count;

    for (size_t i = 0; i < thread_count; i++) {
        Thread *thread = &new_threads[i];
        thread->tid = new_process->next_available_tid++;
        thread->process = new_process;
        thread->kernel_stack_ptr = alloc_kernel_stack();
        if (thread->kernel_stack_ptr == nullptr) {
            panic("Failed to allocate kernel stack for thread #%u, and the cleanup logic is not implemented yet", i);
        }
        thread->state = ThreadState::Suspended;
        new_process->threads.data[i] = thread;

        thread->iframe = static_cast<InterruptFrame*>(alloc_thread_user_stack(&new_process->address_space, i));
        if (thread->iframe == nullptr) {
            panic("Failed to allocate user stack for thread #%u, and the cleanup logic is not implemented yet", i);
        }

        register_thread(thread);
    }

    return new_process;
}

Thread  *cpu_current_thread()    { return s_current_thread; }
Process *cpu_current_process()   { return cpu_current_thread()->process; }

void create_first_process(void (*entrypoint)(void))
{
    Process *stage2 = alloc_process("kernel", 1);
    kassert(stage2 != nullptr);

    Thread *thread = stage2->threads.data[0];
    arch_create_initial_kernel_stack(&thread->kernel_stack_ptr, reinterpret_cast<uintptr_t>(thread->iframe), reinterpret_cast<uintptr_t>(entrypoint), true);
    thread->state = ThreadState::Runnable;

    // timer_install_scheduler_callback(5, scheduler_step);
}

void scheduler_start()
{
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
            LOG("[SCHED]: Context switching to %s[%d]\n", thread->process->name, thread->tid);
            
            // This should be protected someway if we want to support multicore
            // otherwise 2 cores could end up scheduling the same thread using the same kernel stack
            vm_switch_address_space(thread->process->address_space);
            s_current_thread = thread;
            arch_context_switch(&s_scheduler_ctx, reinterpret_cast<ContextSwitchFrame*>(thread->kernel_stack_ptr));
        }
    }
}

void sys$yield()
{
    arch_context_switch(reinterpret_cast<ContextSwitchFrame**>(&s_current_thread->kernel_stack_ptr), s_scheduler_ctx);
}
