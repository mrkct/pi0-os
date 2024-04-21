#include <api/syscalls.h>
#include <kernel/datetime.h>
#include <kernel/device/sd.h>
#include <kernel/device/systimer.h>
#include <kernel/device/videocore.h>
#include <kernel/input/keyboard.h>
#include <kernel/input/miniuart_keyboard.h>
#include <kernel/input/virtinput_keyboard.h>
#include <kernel/device/ramdisk.h>
#include <kernel/device/gpio.h>
#include <kernel/vfs/vfs.h>
#include <kernel/vfs/fat32/fat32.h>
#include <kernel/vfs/sysfs/sysfs.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/lib/memory.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/physicalalloc.h>
#include <kernel/memory/vm.h>
#include <kernel/syscall/syscalls.h>
#include <kernel/task/scheduler.h>
#include <kernel/windowmanager/windowmanager.h>
#include <kernel/timer.h>
#include <stddef.h>
#include <stdint.h>


extern "C" void kernel_main(uint32_t, uint32_t, uint32_t)
{
    using namespace kernel;

    interrupt_init();

    kprintf("Hello, kernel world!\n");
    kprintf("int test. negative value: %d, positive value: %d\n", -1, 1);
    kprintf("unsigned int test. value: %u\n", 1u);
    kprintf("long test. negative value: %ld, positive value: %ld\n", -1l, 1l);
    kprintf("unsigned long test. value: %lu\n", 1ul);
    kprintf("sizeof(unsigned long) = %d   sizeof(int) = %d\n", sizeof(unsigned long), sizeof(unsigned int));
    kprintf("hex test. value: %x\n", 0xabcd1234);
    kprintf("pointer: %p\n", (void*)0x5678);
    kprintf("string: %s\n", "Hello, world!");
    kprintf("binary: %b\n", 0b1101010);

    uint32_t board_revision, firmware_revision;
    MUST(get_board_revision(board_revision));
    MUST(get_firmware_revision(firmware_revision));
    kprintf("firmware: %x board: %s\n", firmware_revision, get_display_name_from_board_revision_id(board_revision));

    uint32_t clock_rate;
    MUST(get_clock_rate(ClockId::EMMC, clock_rate));
    kprintf("emmc clock rate: %dHz\n", clock_rate);
    MUST(get_clock_rate(ClockId::ARM, clock_rate));
    kprintf("arm clock rate: %dHz\n", clock_rate);

    /**
     * On bare-metal we want to wait until we attach with the debugger.
     * For convenience, if the GPIO pin below is high during boot we
     * start spinning to give us time to attach with a debugger.
     * 
     * Tip: Your VCC line from the UART-USB converter is nice to use
     * for this purpose...
    */
    static constexpr int HALT_PIN = 4;
    gpio_set_pin_pull_up_down_state(HALT_PIN, PullUpDownState::PullDown);
    gpio_set_pin_function(HALT_PIN, PinFunction::Input);
    if (gpio_read_pin(HALT_PIN) == 1)
        kprintf("Halt pin %d is high, make it low to continue booting\n", HALT_PIN);
    while (gpio_read_pin(HALT_PIN) == 1);


    kprintf("Initializing physical page allocator...");
    MUST(physical_page_allocator_init(450 * 1024 * 1024));
    kprintf("done\n");
    kprintf("Initializing virtual memory manager...");
    MUST(vm_init_kernel_address_space());
    kprintf("done\n");

    MUST(kheap_init());

    Framebuffer &fb = get_main_framebuffer();
    if (!allocate_videocore_framebuffer(fb).is_success()) {
        MUST(allocate_simulated_framebuffer(fb));
    }

    
    static Storage fs_storage;
    if (ramdisk_probe()) {
        kprintf("Detected ramdisk in kernel image\r\n");
        MUST(ramdisk_init(fs_storage));
    } else {
        kprintf("No ramdisk found, using sd card instead\r\n");
        MUST(sdhc_init());
        kassert(sdhc_contains_card());
        
        static SDCard card;
        kprintf("Initializing sdhc\n");
        MUST(sdhc_initialize_inserted_card(card));
        MUST(sd_storage_interface(card, fs_storage));
        kprintf("sdhc card initialized\n");
    }

    kprintf("Initializing FAT32 filesystem\n");
    static Filesystem fat32_fs;
    MUST(fat32_create(fat32_fs, fs_storage));
    vfs_mount("", &fat32_fs);

    kprintf("Initializing sysfs\n");
    Filesystem *sysfs;
    MUST(sysfs_init(sysfs));
    vfs_mount("/sys", sysfs);

    kprintf("Trying to figure out the current time...\n");
    datetime_init();
    DateTime dt;
    datetime_read(dt);
    kprintf("Current time: %d-%d-%d %d:%d:%d\n", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    kprintf("Initializing syscalls\n");
    syscall_init();

    kprintf("Initializing systimer\n");
    systimer_init();

    kprintf("Figuring out available input devices\n");
    aux_init();
    if (!virtinput_keyboard_init().is_success()) {
        kprintf("No virtinput keyboard found, using miniuart keyboard instead\n");
        miniuart_keyboard_init();
    }

    kprintf("Initializing timer subsystem\n");
    timer_init();

    kprintf("Initializing scheduler\n");
    scheduler_init();
    
    {
        char const* const args[] = {"winman"};
        api::PID pid;
        kprintf("Loading task %s\n", args[0]);
        MUST(task_create_kernel_thread(pid, "winman", array_size(args), args, wm_task_entry));
    }
    
    {
        char const * const args[] = {"/bina/term"};
        api::PID pid;
        kprintf("Loading task %s\n", args[0]);
        MUST(task_load_user_elf_from_path(pid, args[0], array_size(args), args));
    }

    kprintf("Starting scheduler\n");
    scheduler_begin();

    panic("Should not reach here");
}
