#include <kernel/device/sd.h>
#include <kernel/device/systimer.h>
#include <kernel/device/uart.h>
#include <kernel/device/videoconsole.h>
#include <kernel/device/videocore.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <kernel/memory/pagealloc.h>
#include <stddef.h>
#include <stdint.h>

static kernel::VideoConsole vc;

extern "C" void kernel_main(uint32_t, uint32_t, uint32_t)
{
    using namespace kernel;

    auto uart = uart_device();

    uart.init(uart.data);

    kprintf("Hello, kernel world!\n");
    kprintf("int test. negative value: %d, positive value: %d\n", -1, 1);
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

    MUST(sdhc_init());
    if (sdhc_contains_card()) {
        SDCard card;
        MUST(sdhc_initialize_inserted_card(card));
        kprintf("sdhc card initialized\n");

        uint8_t first_three_blocks[3 * 512];
        MUST(sd_read_block(card, 0, 3, first_three_blocks));

        for (size_t i = 0; i < 3; ++i) {
            kprintf("\tblock %d: ", i);
            for (size_t j = 0; j < 32; ++j)
                kprintf("%c", first_three_blocks[i * 512 + j]);
            kprintf("\n");
        }
    }

    Framebuffer fb;
    MUST(allocate_framebuffer(fb));

    MUST(videoconsole_init(vc, fb, 24, 32));
    kprintf_video_init(vc);

    interrupt_init();
    interrupt_install_swi_handler(123, [](auto*) {
        kprintf("BEEP!\n");
    });

    kprintf("Calling software interrupt...\n");
    asm volatile("swi #123");
    kprintf("Returned from software interrupt!\n");

    page_allocator_init();

    systimer_init();
    interrupt_enable();

    systimer_exec_after(3 * 1000000, []() {
        kprintf("TIMER TRIGGERED!\n");
    });

    uint64_t start = systimer_get_ticks();
    int count = 0;
    while (true) {
        while (systimer_get_ticks() - start < 1000000)
            ;
        kprintf("tick. %d\n", count);
        start = systimer_get_ticks();
        count++;
    }

    while (1)
        ;
}
