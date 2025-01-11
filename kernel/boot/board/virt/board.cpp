#include <stddef.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/sizes.h>
#include <kernel/boot/board/board.h>
#include <kernel/boot/misc.h>


static constexpr uintptr_t  RAM_START = 0x40000000;
static constexpr uint32_t   RAM_SIZE = 128 * _1MB;

static constexpr uintptr_t UART_BASE = 0x9000000;
static constexpr uintptr_t REG_DR = UART_BASE + 0x00;
static constexpr uintptr_t REG_FR = UART_BASE + 0x18;
static constexpr uintptr_t REG_IBRD = UART_BASE + 0x24;
static constexpr uintptr_t REG_FBRD = UART_BASE + 0x28;
static constexpr uintptr_t REG_LCRH = UART_BASE + 0x2C;
static constexpr uintptr_t REG_CR = UART_BASE + 0x30;
static constexpr uintptr_t REG_ICR = UART_BASE + 0x44;


void board_early_putchar(char c)
{
    static constexpr uint32_t TRANSMIT_FIFO_FULL = 1 << 5;
    while (ioread32(REG_FR) & TRANSMIT_FIFO_FULL)
        ;
    iowrite32(REG_DR, c);
}

void board_early_console_init()
{
    iowrite32(REG_CR, 0x0);         // Turn off UART
    iowrite32(REG_ICR, 0x7ff);      // Clear all IRQs
    
    // 4. Set integer & fractional part of baud rate
    //    Note: doesn't matter, its not a real UART...
    iowrite32(REG_IBRD, 0);
    iowrite32(REG_FBRD, 0);

    static constexpr uint32_t WORD_LENGTH_EIGHT_BITS = 0b11 << 5;
    static constexpr uint32_t FIFO_ENABLED = 1 << 4;
    iowrite32(REG_LCRH, FIFO_ENABLED | WORD_LENGTH_EIGHT_BITS);

    static constexpr uint32_t ENABLE_UART0 = 1 << 0;
    static constexpr uint32_t ENABLE_TRANSMIT = 1 << 8;
    static constexpr uint32_t ENABLE_RECEIVE = 1 << 9;
    iowrite32(REG_CR, ENABLE_UART0 | ENABLE_TRANSMIT | ENABLE_RECEIVE);
}


range_t board_early_get_ram_range()
{
    return {RAM_START, RAM_SIZE};
}

range_t board_early_get_bootmem_range()
{
    // TODO: This should look at where the device tree and initrd end
    uintptr_t first_free_addr = RAM_START + 16 * _1MB;
    return {first_free_addr, RAM_SIZE - (first_free_addr - RAM_START)};
}
