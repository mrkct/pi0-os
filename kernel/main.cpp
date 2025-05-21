#include <kernel/boot/boot.h>
#include <kernel/drivers/devicemanager.h>
#include <kernel/kprintf.h>
#include <kernel/scheduler.h>
#include <kernel/timer.h>
#include <kernel/vfs/devfs/devfs.h>
#include <kernel/vfs/pipefs/pipefs.h>
#include <kernel/vfs/ptyfs/ptyfs.h>
#include <kernel/vfs/tempfs/tempfs.h>
#include <kernel/vfs/vfs.h>
#include <api/syscalls.h>


static void proc1();


extern "C" void kernel_main(BootParams const *boot_params)
{
    int rc = 0;

    vm_early_init(boot_params);
    devicemanager_init_kernel_log_device(boot_params);
    kprintf_set_puts_func([](const char *line, size_t length) {
        auto *console = devicemanager_get_kernel_log_device();
        console->write((const uint8_t*) line, length);
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
    kassert(rc == 0);

    Filesystem *pipefs;
    rc = pipefs_create(&pipefs);
    if (rc < 0) {
        panic("Failed to create pipefs: %d\n", rc);
    }
    rc = vfs_mount("/pipe/", *pipefs);
    kassert(rc == 0);

    Filesystem *ptyfs;
    rc = ptyfs_create(&ptyfs);
    if (rc < 0) {
        panic("Failed to create ptyfs: %d\n", rc);
    }
    rc = vfs_mount("/dev/pty/", *ptyfs);
    kassert(rc == 0);

    Filesystem *tmpfs;
    rc = tempfs_create(&tmpfs, 8 * _1MB);
    if (rc < 0) {
        panic("Failed to create tmpfs: %d\n", rc);
    }
    rc = vfs_mount("/tmp/", *tmpfs);
    kassert(rc == 0);

    kprintf("Running the first process...\n");
    create_first_process(proc1);
    scheduler_start();
}

#ifndef TEST_PROCESS

static void proc1()
{
    int rc = 0;
    const char * const argv[] = { "/bina/init", nullptr };
    const char * const envp[] = { nullptr };
    api::sys_execve("/bina/init", argv, envp);
    panic("something went wrong with execve: %d\n", rc);
}

#else

static void wait(int secs)
{
    api::sys_millisleep(secs * 1000);
}

static void proc1()
{
    int rc = 0;
    const char * const argv[] = { "/bina/init", nullptr };
    const char * const envp[] = { nullptr };

    kprintf("I am main\n");

    int pid = api::sys_fork();
    if (pid == 0) {
        rc = api::sys_execve("/bina/init", argv, envp);
        kprintf("something went wrong with execve: %d\n", rc);
        while (true) {
            wait(1);
        }
    } else {
        int write, read;
        rc = api::sys_mkpipe(&write, &read);
        
        pid = api::sys_fork();
        if (pid == 0) {
            while (true) {
                kprintf("Writing to pipe 1st\n");
                api::sys_write(write, "Hello from child (1st)\0", 23);
                wait(1);

                kprintf("Writing to pipe 2nd\n");
                api::sys_write(write, "Hello from child (2nd)\0", 23);
                wait(1);

                kprintf("Writing to pipe 3rd\n");
                api::sys_write(write, "Hello from child (3rd)\0", 23);
                wait(8);
            }
        } else {
            while (true) {
                api::PollFd fds[1];
                char buf[100];
                buf[99] = '\0';

                fds[0].fd = read;
                fds[0].events = F_POLLIN;

                rc = api::sys_poll(fds, 1, 1000);
                if (rc == -ERR_TIMEDOUT) {
                    kprintf("Timed out\n");
                } else {
                    int nread = api::sys_read(read, buf, 100);
                    if (nread > 0)
                        kprintf("Read %d bytes: %s\n", nread, buf);
                }
            }
        }
    }
}

#endif
