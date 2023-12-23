#include <stddef.h>
#include <stdint.h>
#include <stddef.h>


/**
 * THIS CODE IS SPECIFIC FOR THE RASPBERRY PI ZERO W
 *  This is because on the Pi Zero W the uart0 is not actually available
 *  outside because it's reserved for Bluetooth, instead what you have
 *  is the mini-uart. Don't trust what people say online, it's not true
 *  you can reclaim the uart0.
 * 
 * 
 * This file is used to function that are used before the MMU is setup,
 * and therefore all of it's code cannot reference anything outside
 * of this as they are linked at unaccessible addresses since virtual
 * memory is not yet enabled.
 * 
 * This is mostly used for debugging stuff on bare metal.
 * 
 * Note that even if we include some header, we cannot use any of their
 * functions, not even the inline templates as sometimes the compiler
 * doesn't actually inline them.
*/

#define BOOT_TEXT __attribute__((section(".text.start")))
#define BOOT_DATA __attribute__((section(".rodata.start")))

#ifdef BOARD_RASPI2
#define IO_BASE 0x3f000000
#else
#define IO_BASE 0x20000000
#endif

namespace kernel {

static BOOT_TEXT constexpr uintptr_t bcm2835_bus_address_to_physical(uintptr_t addr)
{

    return (addr - 0x7e000000) + IO_BASE;
}

static BOOT_DATA constexpr uintptr_t GPIO_BASE = bcm2835_bus_address_to_physical(0x7e200000);
static BOOT_DATA constexpr uintptr_t GPFSEL1 = GPIO_BASE + 0x04;
static BOOT_DATA constexpr uintptr_t GPPUD = GPIO_BASE + 0x94;
static BOOT_DATA constexpr uintptr_t GPPUDCLK0 = GPIO_BASE + 0x98;
static BOOT_DATA constexpr uintptr_t GPPUDCLK1 = GPIO_BASE + 0x9c;

static BOOT_DATA constexpr uintptr_t AUX_BASE = bcm2835_bus_address_to_physical(0x7e215000);
static BOOT_DATA constexpr uintptr_t AUX_ENABLES = AUX_BASE + 0x04;
static BOOT_DATA constexpr uintptr_t AUX_MU_IO	= AUX_BASE + 0x40;
static BOOT_DATA constexpr uintptr_t AUX_MU_LSR = AUX_BASE + 0x54;
static BOOT_DATA constexpr uintptr_t AUX_MU_CNTL = AUX_BASE + 0x60;
static BOOT_DATA constexpr uintptr_t AUX_MU_STAT = AUX_BASE + 0x64;


static inline BOOT_TEXT void boot_memory_barrier()
{
    asm volatile(
        "mcr p15, 0, r3, c7, c5, 0  \n" // Invalidate instruction cache
        "mcr p15, 0, r3, c7, c5, 6  \n" // Invalidate BTB
        "mcr p15, 0, r3, c7, c10, 4 \n" // Drain write buffer
        "mcr p15, 0, r3, c7, c5, 4  \n" // Prefetch flush
        :
        :
        : "r3");
}

static inline BOOT_TEXT void boot_iowrite32(uintptr_t reg, uint32_t data)
{
    boot_memory_barrier();
    *reinterpret_cast<uint32_t volatile*>(reg) = data;
}

static inline BOOT_TEXT uint32_t boot_ioread32(uintptr_t reg)
{
    boot_memory_barrier();
    return *reinterpret_cast<uint32_t volatile*>(reg);
}

extern "C" BOOT_TEXT void boot_miniuart_putc(char c)
{
    static BOOT_DATA constexpr uint32_t TRANSMITTER_EMPTY = 1 << 5;
    while (!(boot_ioread32(AUX_MU_LSR) & TRANSMITTER_EMPTY))
	    ;
	
    boot_iowrite32(AUX_MU_IO, c);
}

// static BOOT_DATA const char boot_msg[] = "ciaoo";

extern "C" BOOT_TEXT void boot_miniuart_puts(char const *s)
{
    boot_miniuart_putc('!');
    while (*s) {
        boot_miniuart_putc(*s);
        s++;
    }
    boot_miniuart_putc('!');
    boot_miniuart_putc('\r');
    boot_miniuart_putc('\n');
}

extern "C" BOOT_TEXT void boot_init_console_uart()
{
    static BOOT_DATA constexpr uint32_t MINI_UART_ENABLED = 1;
    boot_iowrite32(AUX_ENABLES, MINI_UART_ENABLED);

    static BOOT_DATA constexpr uint32_t TX_ENABLE = 1;
    static BOOT_DATA constexpr uint32_t RX_ENABLE = 1;
    boot_iowrite32(AUX_MU_CNTL, TX_ENABLE | RX_ENABLE);
}

}
