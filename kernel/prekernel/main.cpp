#include <api/syscalls.h>
#include <kernel/datetime.h>
#include <kernel/device/sd.h>
#include <kernel/device/systimer.h>
#include <kernel/device/uart.h>
#include <kernel/device/videoconsole.h>
#include <kernel/device/videocore.h>
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

static kernel::VideoConsole vc;

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

    api::ProcessInfo info;
    api::syscall(api::SyscallIdentifiers::GetProcessInfo, reinterpret_cast<uint32_t>(&info), 0, 0, 0, 0);

    size_t len = kernel::ksprintf(buf, sizeof(buf), "I am %s and my PID is %d\n", info.name, info.pid);
    api::syscall(api::SyscallIdentifiers::DebugLog, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0);

    api::DateTime datetime;
    api::syscall(api::SyscallIdentifiers::GetDateTime, reinterpret_cast<uint32_t>(&datetime), 0, 0, 0, 0);

    len = kernel::ksprintf(buf, sizeof(buf), "The date is %d-%d-%d %d:%d:%d\n",
        datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second);
    api::syscall(api::SyscallIdentifiers::DebugLog, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0);

    api::Stat stat;
    char const* pathname = "/HELLO.TXT";
    if (0 == api::syscall(api::SyscallIdentifiers::Stat, reinterpret_cast<uint32_t>(pathname), klib::strlen(pathname), reinterpret_cast<uint32_t>(&stat), 0, 0)) {
        len = kernel::ksprintf(buf, sizeof(buf), "- isDirectory: %s \n- size: %lu bytes\n", stat.is_directory ? "true" : "false", stat.size);
        api::syscall(api::SyscallIdentifiers::DebugLog, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0);
    } else {
        len = kernel::ksprintf(buf, sizeof(buf), "stat syscall failed. Skipping file system calls...\n");
        api::syscall(api::SyscallIdentifiers::DebugLog, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0);
    }

    len = kernel::ksprintf(buf, sizeof(buf), "Ticks: %lu\n", datetime.ticks_since_boot);
    api::syscall(api::SyscallIdentifiers::DebugLog, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0);

    api::syscall(api::SyscallIdentifiers::Sleep, 10000, 0, 0, 0, 0);

    api::syscall(api::SyscallIdentifiers::GetDateTime, reinterpret_cast<uint32_t>(&datetime), 0, 0, 0, 0);
    len = kernel::ksprintf(buf, sizeof(buf), "Ticks: %lu\n", datetime.ticks_since_boot);
    api::syscall(api::SyscallIdentifiers::DebugLog, reinterpret_cast<uint32_t>(buf), len, 0, 0, 0);

    api::syscall(api::SyscallIdentifiers::Exit, 0, 0, 0, 0, 0);
    kassert_not_reached();
}

extern "C" void kernel_main(uint32_t, uint32_t, uint32_t)
{
    using namespace kernel;

    auto uart = uart_device();

    uart.init(uart.data);

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

    kprintf("Initializing physical page allocator...");
    MUST(physical_page_allocator_init(450 * 1024 * 1024));
    kprintf("done\n");
    kprintf("Initializing virtual memory manager...");
    MUST(vm_init_kernel_address_space());
    kprintf("done\n");

    Framebuffer fb;
    MUST(allocate_framebuffer(fb));

    MUST(videoconsole_init(vc, fb, 24, 32));
    kprintf_video_init(vc);

    interrupt_init();

    MUST(kheap_init());

    char* test;
    MUST(kmalloc(1024, test));
    for (int i = 0; i < 1024; ++i)
        test[i] = 'a';
    MUST(kfree(test));

    MUST(sdhc_init());
    if (sdhc_contains_card()) {
        static SDCard card;
        static Storage card_storage;
        MUST(sdhc_initialize_inserted_card(card));
        MUST(sd_storage_interface(card, card_storage));
        kprintf("sdhc card initialized\n");

        static Filesystem fs;
        MUST(fat32_create(fs, card_storage));
        MUST(fs.init(fs));
        fs_set_root(&fs);
    }

    datetime_init();
    syscall_init();
    systimer_init();

    timer_init();
    scheduler_init();
    Task *A, *B;
    MUST(task_create_kernel_thread(A, "A", task_A));

    // FIXME: There's a very hard to find bug where having this
    // task run can cause A to crash. Will investigate later
    // MUST(task_create_kernel_thread(B, "B", task_A));

    timer_exec_after(
        1000, [](void* id) {
            kprintf("Timer %d expired!\n", reinterpret_cast<uint32_t>(id));
        },
        (void*)0x1);
    timer_exec_after(
        2000, [](void* id) {
            kprintf("Timer %d expired!\n", reinterpret_cast<uint32_t>(id));
        },
        (void*)0x2);
    timer_exec_after(
        3000, [](void* id) {
            kprintf("Timer %d expired!\n", reinterpret_cast<uint32_t>(id));
        },
        (void*)0x3);
    timer_exec_after(
        4000, [](void* id) {
            kprintf("Timer %d expired!\n", reinterpret_cast<uint32_t>(id));
        },
        (void*)0x4);
    timer_exec_after(
        5000, [](void* id) {
            kprintf("Timer %d expired!\n", reinterpret_cast<uint32_t>(id));
        },
        (void*)0x5);

    scheduler_begin();

    panic("Should not reach here");
}
