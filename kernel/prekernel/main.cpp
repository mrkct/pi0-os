#include <kernel/device/sd.h>
#include <kernel/device/uart.h>
#include <kernel/device/videocore.h>
#include <kernel/interrupt.h>
#include <kernel/kprintf.h>
#include <stddef.h>
#include <stdint.h>

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

    for (size_t i = 0; i < fb.height; ++i)
        for (size_t j = 0; j < fb.width; ++j)
            fb.address[i * fb.width + j] = i * j;

    install_software_interrupt(123, [](InterruptFrame*) {
        kprintf("BEEP!\n");
    });

    kprintf("Calling software interrupt...\n");
    asm volatile("swi #123");
    kprintf("Returned from software interrupt!\n");

    while (1)
        ;
}
