#include <kernel/device/uart.h>
#include <kernel/kprintf.h>
#include <stddef.h>
#include <stdint.h>

extern "C" void kernel_main(uint32_t, uint32_t machine_id, uint32_t atags)
{
    using namespace kernel;
    (void)machine_id;
    (void)atags;

    auto uart = uart_device();

    uart.init(uart.data);

    kprintf("Hello, kernel world!\n");
    kprintf("machine_id: %x\n", machine_id);
    kprintf("int test. negative value: %d, positive value: %d\n", -1, 1);
    kprintf("hex test. value: %x\n", 0xabcd1234);
    kprintf("pointer: %p\n", (void*)0x5678);
    kprintf("string: %s\n", "Hello, world!");
    kprintf("binary: %b\n", 0b1101010);

    while (1)
        ;
}
