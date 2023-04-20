#include <stdint.h>
#include <stddef.h>
#include <kernel/device/uart.h>


extern "C" void kernel_main(uint32_t, uint32_t machine_id, uint32_t atags)
{
    using namespace kernel;
    (void) machine_id;
    (void) atags;

    auto uart = uart_device();
    
    uart.init(uart.data);

    auto const& puts = [](char const* str) -> Error {
        auto uart = uart_device();
        for (size_t i = 0; str[i] != '\0'; i++)
            TRY(uart.write(&uart.data, (unsigned char)str[i]));
        
        return Success;
    };

    puts("Hello, kernel world!\n");

    while(1);
}
