#include <kernel/device/aux.h>
#include <kernel/device/io.h>
#include <kernel/interrupt.h>


namespace kernel {

static constexpr uint32_t AUX_IRQ = 29;

static constexpr uintptr_t AUX_BASE = bcm2835_bus_address_to_physical(0x7e215000);
static constexpr uintptr_t AUX_ENABLES = AUX_BASE + 0x04;

/**
 * These 2 are not real registers on the BCM2835 and are not available in normal QEMU.
 * This is a custom device that is only available when running my custom fork of QEMU.
*/
static constexpr uintptr_t BIT_KBD_INT      = 1 << 30;
static constexpr uintptr_t BIT_MOUSE_INT    = 1 << 31;
static constexpr uintptr_t AUX_KEYBD_DATA_REG = AUX_BASE + 0x8;
static constexpr uintptr_t AUX_MOUSE_DATA_REG = AUX_BASE + 0xc;

static constexpr uintptr_t AUX_MU_IO = AUX_BASE + 0x40;
static constexpr uintptr_t AUX_MU_IER = AUX_BASE + 0x44;
static constexpr uintptr_t AUX_MU_IIR = AUX_BASE + 0x48;
static constexpr uintptr_t AUX_MU_LSR = AUX_BASE + 0x54;
static constexpr uintptr_t AUX_MU_CNTL = AUX_BASE + 0x60;
static constexpr uintptr_t AUX_MU_STAT = AUX_BASE + 0x64;

static void (*g_rx_irq_callback)(uint8_t) = nullptr;
static void (*g_virtinput_kbd_callback)(uint32_t, bool) = nullptr;
static void (*g_virtinput_mouse_callback)(int8_t, int8_t, uint16_t) = nullptr;

static void aux_irq_handler(auto*)
{
    static constexpr uint32_t IRQ_PENDING = 1;
    if ((ioread32<uint32_t>(AUX_MU_IIR) & IRQ_PENDING) != 0)
        return;
        
    uint32_t irq_source = ioread32<uint32_t>(AUX_MU_IIR);
    
    // Check if it is a UART "Receiver holds valid byte" interrupt
    if ((irq_source >> 1 & 0b11) == 0b10) {
        uint8_t data = ioread32<uint32_t>(AUX_MU_IO) & 0xff;
        if (g_rx_irq_callback)
            g_rx_irq_callback(data);
    }

    if (irq_source & BIT_KBD_INT) {
        uint32_t data = ioread32<uint32_t>(AUX_KEYBD_DATA_REG);
        uint32_t keycode = data >> 8 & 0xffffff;
        bool pressed = data & 1;
        if (g_virtinput_kbd_callback)
            g_virtinput_kbd_callback(keycode, pressed);
    }
    if (irq_source & BIT_MOUSE_INT) {
        uint32_t data = ioread32<uint32_t>(AUX_MOUSE_DATA_REG);
        kprintf("rx mouse: %p\n", data);
        auto rel_x = (int8_t)(data >> 16 & 0xff);
        auto rel_y = (int8_t)(data >> 8 & 0xff);
        uint16_t buttons = data & 0xffff;
        if (g_virtinput_mouse_callback)
            g_virtinput_mouse_callback(rel_x, rel_y, buttons);
    }
}

void miniuart_putc(unsigned char c)
{
    static constexpr uint32_t TRANSMITTER_EMPTY = 1 << 5;
    while (!(ioread32<uint32_t>(AUX_MU_LSR) & TRANSMITTER_EMPTY))
	    ;
	
    iowrite32<uint32_t>(AUX_MU_IO, c);
}

extern "C" void miniuart_puts(const char *s)
{
    while (*s) {
        miniuart_putc(*s);
        s++;
    }
    miniuart_putc('\r');
    miniuart_putc('\n');
}

void aux_init()
{
    interrupt_install_irq1_handler(AUX_IRQ, aux_irq_handler);
}

void miniuart_enable_rx_irq(void (*callback)(uint8_t))
{
    g_rx_irq_callback = callback;
    
    // Check errata: TX and RX bits are swapped, and you also need to set bits 3:2
    static constexpr uint32_t RX_IRQ_ENABLE = 1 | (1 << 2);

    auto ier = ioread32<uint32_t>(AUX_MU_IER);
    ier |= RX_IRQ_ENABLE;
    iowrite32<uint32_t>(AUX_MU_IER, ier);
}

bool virtinput_is_available()
{
    uint32_t cpuid;
    asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r"(cpuid));
    return (cpuid & 0xff000000) == 0x42000000;
}

void virtinput_enable_keyboard_irq(void (*callback)(uint32_t keycode, bool pressed))
{
    kassert(virtinput_is_available());

    g_virtinput_kbd_callback = callback;
    auto ier = ioread32<uint32_t>(AUX_MU_IER);
    ier |= BIT_KBD_INT;
    iowrite32<uint32_t>(AUX_MU_IER, ier);
}

void virtinput_enable_mouse_irq(void (*callback)(int8_t, int8_t, uint16_t))
{
    kassert(virtinput_is_available());

    g_virtinput_mouse_callback = callback;
    auto ier = ioread32<uint32_t>(AUX_MU_IER);
    ier |= BIT_MOUSE_INT;
    iowrite32<uint32_t>(AUX_MU_IER, ier);
}


}
