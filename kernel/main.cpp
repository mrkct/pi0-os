#include <kernel/boot/boot.h>
#include <kernel/drivers/devicemanager.h>
#include <kernel/kprintf.h>
#include <kernel/scheduler.h>
#include <kernel/timer.h>
#include <kernel/vfs/devfs/devfs.h>
#include <kernel/vfs/vfs.h>
#include <api/syscalls.h>


static void proc1();


extern "C" void kernel_main(BootParams const *boot_params)
{
    int rc = 0;

    vm_early_init(boot_params);
    devicemanager_init_kernel_log_device(boot_params);
    kprintf_set_putchar_func([](char c) {
        auto *console = devicemanager_get_kernel_log_device();
        console->write(0, (uint8_t*) &c, 1);
    });

    kprintf("Booting...\n");
    kprintf("Boot params at %p\n", boot_params);
    kprintf(" RAM start: %p - %u bytes\n", boot_params->ram_start, boot_params->ram_size);
    kprintf(" Reserved boot memory: %u bytes\n", boot_params->bootmem_size);
    kprintf(" initrd: %p - %u bytes\n", boot_params->initrd_start, boot_params->initrd_size);
    kprintf(" device tree: %p - %u bytes\n", boot_params->device_tree_start, boot_params->device_tree_size);

    kprintf("Initializing physical memory allocator...\n");
    physical_page_allocator_init(boot_params);
    
    kprintf("Completing virtual memory subsystem initialization...\n");
    vm_init();

    kprintf("Initializing interrupt subsystem...\n");
    irq_init();

    kprintf("Initializing kernel heap...\n");
    kheap_init();

    kprintf("Discovering available devices...\n");
    devicemanager_load_available_peripherals(boot_params);

    kprintf("Initializing timer subsystem...\n");
    timer_init();

    kprintf("Mounting root filesystem...\n");
    auto *root_device = devicemanager_get_root_block_device();
    kassert(root_device != nullptr);

    auto *root_filesystem = fs_detect_and_create(*root_device);
    kassert(root_filesystem != nullptr);

    rc = vfs_mount("/", *root_filesystem);
    if (rc < 0) {
        panic("Failed to mount root filesystem: %d\n", rc);
    }

    Filesystem *devfs;
    rc = devfs_create(&devfs);
    if (rc < 0) {
        panic("Failed to create devfs: %d\n", rc);
    }
    rc = vfs_mount("/dev/", *devfs);

    kprintf("Running the first process...\n");
    create_first_process(proc1);
    scheduler_start();
}

static void wait(int secs)
{
    auto *timer = devicemanager_get_system_timer_device();
    uint64_t last = timer->ticks();
    
    while (timer->ticks() - last < (secs * 1000 *timer->ticks_per_ms())) {
        cpu_relax();
        api::sys_yield();
    }
}

static void proc1()
{
    int rc = 0;
    const char *argv[] = { "/bina/init", nullptr };
    const char *envp[] = { nullptr };

    kprintf("I am main\n");

    int pid = api::sys_fork();
    if (pid == 0) {
        rc = api::sys_execve("/bina/init", argv, envp);
        kprintf("something went wrong with execve: %d\n", rc);
        while (true) {
            wait(1);
        }
    } else {
        while (true) {
            kprintf("I am main with child %d\n", pid);
            wait(1);
        }
    }
}
