#include <kernel/boot/boot.h>
#include <kernel/drivers/devicemanager.h>
#include <kernel/kprintf.h>
#include <kernel/timer.h>

#include <kernel/drivers/irqc/gic2.h>


extern "C" void kernel_main(BootParams const *boot_params)
{
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

    kprintf("Enabling interrupts...\n");
    irq_enable();
    
    uint32_t counter = 0;
    timer_exec_periodic(1000, [](void *_counter) {
        uint32_t *counter = (uint32_t*) _counter;
        kprintf("Tick %u..\n", *counter);
        (*counter)++;
    }, &counter);

    kprintf("Starting kernel...\n");
    while (1) {
        cpu_relax();
    }
}
