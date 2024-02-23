#include <api/syscalls.h>
#include <kernel/datetime.h>
#include <kernel/device/sd.h>
#include <kernel/device/systimer.h>
#include <kernel/device/videocore.h>
#include <kernel/device/keyboard.h>
#include <kernel/device/ramdisk.h>
#include <kernel/device/gpio.h>
#include <kernel/filesystem/fat32/fat32.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/lib/string.h>
#include <kernel/memory/kheap.h>
#include <kernel/memory/physicalalloc.h>
#include <kernel/memory/vm.h>
#include <kernel/syscall/syscalls.h>
#include <kernel/task/scheduler.h>
#include <kernel/timer.h>
#include <stddef.h>
#include <stdint.h>


static void __attribute__((unused)) fs_tree(kernel::Filesystem& fs)
{
    using namespace kernel;

    static void (*helper)(Filesystem&, Directory&, size_t) = [](auto& fs, auto& dir, auto tab_size) {
        using namespace kernel;

        DirectoryEntry entry;
        while (fs.directory_next_entry(dir, entry).is_success()) {
            for (size_t i = 0; i < tab_size; i++)
                kernel::kprintf(" ");

            kernel::kprintf("%s\n", entry.name);
            if (entry.type == DirectoryEntry::Type::Directory && entry.name[0] != '.') {
                Directory sub_dir;
                MUST(fs.open_directory_entry(entry, sub_dir));
                helper(fs, sub_dir, tab_size + 2);
            }
        }
    };

    Directory root_dir;
    MUST(fs.root_directory(fs, root_dir));
    helper(fs, root_dir, 0);
}

static void task_A()
{
    char buf[1024];

#define taskprintf(format, args...)                                                                     \
    do {                                                                                                \
        len = kernel::ksprintf(buf, sizeof(buf), format, ##args);                                       \
        syscall(SyscallIdentifiers::SYS_DebugLog, nullptr, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0); \
    } while (0)

    size_t len;
    uint32_t result;

    ProcessInfo info;
    syscall(SyscallIdentifiers::SYS_GetProcessInfo, nullptr, reinterpret_cast<uint32_t>(&info), 0, 0, 0, 0);

    taskprintf("I am %s and my PID is %d\n", info.name, info.pid);

    DateTime datetime;
    syscall(SyscallIdentifiers::SYS_GetDateTime, nullptr, reinterpret_cast<uint32_t>(&datetime), 0, 0, 0, 0);

    taskprintf("The date is %d-%d-%d %d:%d:%d\n",
        datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second);

    Stat stat;
    char const* pathname = "/README.MD";
    if (0 == syscall(SyscallIdentifiers::SYS_Stat, nullptr, reinterpret_cast<uint32_t>(pathname), reinterpret_cast<uint32_t>(&stat), 0, 0, 0)) {
        taskprintf("- isDirectory: %s \n- size: %lu bytes\n", stat.is_directory ? "true" : "false", stat.size);

        int32_t fd;
        result = syscall(
            SyscallIdentifiers::SYS_OpenFile,
            reinterpret_cast<uint32_t*>(&fd),
            reinterpret_cast<uint32_t>(pathname),
            klib::strlen(pathname),
            MODE_READ | MODE_WRITE | MODE_APPEND,
            0, 0);
        if (result == 0) {
            uint8_t data[64];
            uint32_t count = 1;
            taskprintf("========== Content of file ==========\n");
            while (count > 0) {
                syscall(SyscallIdentifiers::SYS_ReadFile, &count, fd, reinterpret_cast<uint32_t>(data), sizeof(data) - 1, 0, 0);
                data[count] = '\0';
                taskprintf("%s", data);
            }
            taskprintf("\n========= End of file ==========\n");

            int rc = syscall(SyscallIdentifiers::SYS_CloseFile, nullptr, fd, 0, 0, 0, 0);
            if (rc < 0)
                taskprintf("failed to close file. rc=%d\n", rc);
        } else {
            taskprintf("failed to open %s. rc= %d\n", pathname, fd);
        }
    } else {
        taskprintf("stat syscall failed. Skipping file system calls...\n");
    }

    taskprintf("Ticks: %lu\n", datetime.ticks_since_boot);

    syscall(SyscallIdentifiers::SYS_Sleep, nullptr, 10000, 0, 0, 0, 0);

    syscall(SyscallIdentifiers::SYS_GetDateTime, nullptr, reinterpret_cast<uint32_t>(&datetime), 0, 0, 0, 0);
    taskprintf("Ticks: %lu\n", datetime.ticks_since_boot);

    PID pid;
    result = syscall(
        SyscallIdentifiers::SYS_SpawnProcess,
        reinterpret_cast<uint32_t*>(&pid),
        reinterpret_cast<uint32_t>("/bina/clock"),
        0, 0, 0, 0
    );
    taskprintf("Clock PID: %u\n", pid);

    syscall(SyscallIdentifiers::SYS_Exit, nullptr, 0, 0, 0, 0, 0);
    kassert_not_reached();
}

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

    static Filesystem fs;
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

    MUST(fat32_create(fs, fs_storage));
    MUST(fs.init(fs));
    fs_set_root(&fs);

    datetime_init();
    syscall_init();
    systimer_init();
    init_user_keyboard(KeyboardSource::MiniUart);

    timer_init();
    scheduler_init();

    PID pid;
    // MUST(task_create_kernel_thread(pid, "A", 0, {}, task_A));

    const char *args[] = {"/bina/shell"};
    MUST(task_load_user_elf_from_path(pid, "/bina/shell", 1, args));

    // FIXME: There's a very hard to find bug where having this
    // task run can cause A to crash. Will investigate later
    // MUST(task_create_kernel_thread(B, "B", task_A));

    timer_exec_after(
        1000, [](void* id) {
            kprintf("Timer %d expired!\n", reinterpret_cast<uint32_t>(id));
        },
        (void*)0x1);

    scheduler_begin();

    panic("Should not reach here");
}
