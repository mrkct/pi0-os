#include <kernel/memory/bootalloc.h>

#include "devicemanager.h"

#include "block/virtioblk.h"

#include "char/bcm2835_aux_uart.h"
#include "char/bcm2835_gpio.h"
#include "char/pl011.h"
#include "char/pl031.h"
#include "char/virtiogpu.h"
#include "char/virtioinput.h"

#include "irqc/bcm2835_irqc.h"
#include "irqc/gic2.h"

#include "timer/armv7timer.h"
#include "timer/bcm2835_systimer.h"


static void raspi0_init_log_device();
static void raspi0_load_peripherals();

static void virt_init_log_device();
static void virt_load_peripherals();


static Device *s_devices[64];
static struct {
    InterruptController *irqc;
    CharacterDevice *kernel_log;
    SystemTimer *systimer;
    BlockDevice *storage;
} s_defaults;

struct Driver {
    const char *compatible;
    size_t required_space;
    Device *(*load)(const char *compatible, void *storage, void const *config);
};

template<typename T>
static constexpr Driver basic_init(const char *compatible)
{
    return Driver {
        .compatible = compatible,
        .required_space = sizeof(T),
        .load = [](const char*, void *storage, void const *config) -> Device* { return new (storage) T(reinterpret_cast<T::Config const*>(config)); },
    };
}

static constexpr Driver s_drivers[] = {
    basic_init<PL011UART>("arm,pl011"),
    basic_init<BCM2835GPIOController>("brcm,bcm2835-gpio"),
    basic_init<BCM2835AuxUART>("brcm,bcm2835-aux-uart"),
    basic_init<BCM2835InterruptController>("brcm,bcm2835-armctrl-ic"),
    basic_init<GlobalInterruptController2>("arm,cortex-a15-gic"),
    basic_init<BCM2835SystemTimer>("brcm,bcm2835-system-timer"),
    basic_init<ARMv7Timer>("arm,armv7-timer"),
    basic_init<PL031>("arm,pl031"),

    // This is not right: the compatible should be "virtio,mmio" which handles
    // all virtio devices, not separate compatibles for each device class
    basic_init<VirtioBlockDevice>("virtioblk,mmio"),
    basic_init<VirtioInputDevice>("virtioinput,mmio"),
    basic_init<VirtioGPU>("virtiogpu,mmio"),
};

static Driver const *find_driver(const char *compatible)
{
    for (size_t i = 0; i < array_size(s_drivers); i++) {
        if (strcmp(s_drivers[i].compatible, compatible) == 0) {
            return &s_drivers[i];
        }
    }

    return nullptr;
}

static void register_device(Device *device)
{
    for (size_t i = 0; i < array_size(s_devices); i++) {
        if (s_devices[i] == nullptr) {
            s_devices[i] = device;
            return;
        }
    }
    panic("No more space for devices");
}

enum class DetectedMachine {
    Raspi0, Virt, Unknown
};

static DetectedMachine detect_machine(BootParams const *boot_params)
{
    if (boot_params->ram_start == 0) {
        return DetectedMachine::Raspi0;
    } else if (boot_params->ram_start == 0x40000000) {
        return DetectedMachine::Virt;
    }
    return DetectedMachine::Unknown;
}

void devicemanager_init_kernel_log_device(BootParams const *boot_params)
{
    switch (detect_machine(boot_params)) {
    case DetectedMachine::Raspi0: {
        raspi0_init_log_device();
        break;
    }
    case DetectedMachine::Virt: {
        virt_init_log_device();
        break;
    }
    case DetectedMachine::Unknown:
        panic("Unknown machine");
    }
}

void devicemanager_load_available_peripherals(BootParams const *boot_params)
{
    switch (detect_machine(boot_params)) {
    case DetectedMachine::Raspi0:
        raspi0_load_peripherals();
        break;
    case DetectedMachine::Virt:
        virt_load_peripherals();
        break;
    case DetectedMachine::Unknown:
        panic("Unknown machine");
    }
}

Device *devicemanager_get_by_name(const char *name)
{
    if (0 == strcmp(name, "kernel_log"))
        return s_defaults.kernel_log;

    for (size_t i = 0; i < array_size(s_devices); i++) {
        if (s_devices[i] != nullptr && strcmp(s_devices[i]->name(), name) == 0) {
            return s_devices[i];
        }
    }
    return nullptr;
}

Device *devicemanager_get_by_devid(uint16_t devid)
{
    FileDevice *device = nullptr;

    for (size_t i = 0; i < array_size(s_devices); i++) {
        if (s_devices[i] == nullptr || !s_devices[i]->is_mountable())
            continue;
        
        device = reinterpret_cast<FileDevice*>(s_devices[i]);
        if (device->dev_id() == devid)
            return device;
    }

    return nullptr;
}

CharacterDevice *devicemanager_get_kernel_log_device() { return s_defaults.kernel_log; }

InterruptController *devicemanager_get_interrupt_controller_device() { return s_defaults.irqc; }

SystemTimer *devicemanager_get_system_timer_device() { return s_defaults.systimer; }

BlockDevice *devicemanager_get_root_block_device() { return s_defaults.storage; }

/////////////////////////////// RASPBERRY PI 0 ////////////////////////////////

static constexpr uintptr_t RASPI0_IOBASE = 0x20000000;

static void raspi0_init_log_device()
{
    const char *gpio_compatible = "brcm,bcm2835-gpio";
    Driver const *gpio_drv = find_driver(gpio_compatible);
    kassert(gpio_drv != nullptr);

    BCM2835GPIOController::Config gpio_config {
        .iobase = RASPI0_IOBASE,
        .offset = 0x00200000,
    };
    GPIOController *gpio_dev = reinterpret_cast<GPIOController*>(gpio_drv->load(gpio_compatible, bootalloc(gpio_drv->required_space), &gpio_config));
    if (gpio_dev && 0 == gpio_dev->init_for_early_boot()) {
        register_device(gpio_dev);

        gpio_dev->configure_pin_pull_up_down(0, 14, GPIOController::PullState::None);
        gpio_dev->configure_pin(0, 14, GPIOController::PinFunction::Alt5);

        gpio_dev->configure_pin_pull_up_down(0, 15, GPIOController::PullState::None);
        gpio_dev->configure_pin(0, 15, GPIOController::PinFunction::Alt5);


        const char *uart_compatible = "brcm,bcm2835-aux-uart";
        Driver const *uart_drv = find_driver(uart_compatible);
        kassert(uart_drv != nullptr);

        BCM2835AuxUART::Config uart_config {
            .iobase = RASPI0_IOBASE,
            .offset = 0x00215000,
            .irq = BCM2835InterruptController::irq(BCM2835InterruptController::Group::Pending1, 29),
        };
        FileDevice *uart_dev = static_cast<FileDevice*>(uart_drv->load(uart_compatible, bootalloc(uart_drv->required_space), &uart_config));
        if (uart_dev && 0 == uart_dev->init_for_early_boot()) {
            register_device(uart_dev);
            s_defaults.kernel_log = reinterpret_cast<CharacterDevice*>(uart_dev);
        }
    }
}

static void raspi0_load_peripherals()
{
    int32_t rc;

    const char *irqc_compatible = "brcm,bcm2835-armctrl-ic";
    kprintf("Initializing interrupt controller (%s)...\n", irqc_compatible);
    Driver const *irqc_drv = find_driver(irqc_compatible);
    BCM2835InterruptController::Config irqc_config {
        .iobase = RASPI0_IOBASE,
        .offset = 0x0000b000
    };
    auto *irqc_dev = reinterpret_cast<InterruptController*>(irqc_drv->load(irqc_compatible, mustmalloc(irqc_drv->required_space), &irqc_config)); 
    rc = irqc_dev->init();
    if (rc != 0)
        panic("Failed to initialize interrupt controller: %d\n", rc);
    register_device(irqc_dev);
    s_defaults.irqc = irqc_dev;

    kprintf("Completing kernel log device initialization...\n");
    rc = s_defaults.kernel_log->init();
    if (rc != 0)
        panic("Failed to initialize kernel log device: %d\n", rc);
    
    kprintf("Initializing system timer...\n");
    Driver const *systimer_drv = find_driver("brcm,bcm2835-system-timer");
    BCM2835SystemTimer::Config systimer_config {
        .address = RASPI0_IOBASE + 0x00003000,
        .clock_frequency = 1000000,
    };
    auto *systimer_dev = reinterpret_cast<SystemTimer*>(systimer_drv->load(irqc_compatible, mustmalloc(systimer_drv->required_space), &systimer_config));
    rc = systimer_dev->init();
    if (rc != 0)
        panic("Failed to initialize system timer: %d\n", rc);
    s_defaults.systimer = systimer_dev;
}

////////////////////////////////// QEMU VIRT //////////////////////////////////

static void virt_init_log_device()
{
    Driver const *driver = find_driver("arm,pl011");
    PL011UART::Config config {
        .physaddr = 0x9000000,
        .irq = 32 + 1,
    };
    auto *console = (CharacterDevice*) driver->load("arm,pl011", bootalloc(driver->required_space), &config);
    if (console && 0 == console->init_for_early_boot()) {
        register_device(console);
        s_defaults.kernel_log = console;
    }
}

static void virt_load_peripherals()
{
    int32_t rc;

    const char *irqc_compatible = "arm,cortex-a15-gic";
    kprintf("Initializing interrupt controller (%s)...\n", irqc_compatible);
    Driver const *irqc_drv = find_driver(irqc_compatible);
    kassert(irqc_drv != nullptr);
    GlobalInterruptController2::Config irqc_config = {
        .distributor_address = 0x08000000,
        .cpu_interface_address = 0x08010000
    };
    auto *irqc_dev = reinterpret_cast<InterruptController*>(irqc_drv->load(irqc_compatible, mustmalloc(irqc_drv->required_space), &irqc_config));
    rc = irqc_dev->init();
    if (rc != 0)
        panic("Failed to initialize interrupt controller: %d\n", rc);
    register_device(irqc_dev);
    s_defaults.irqc = irqc_dev;


    kprintf("Completing kernel log device initialization...\n");
    rc = s_defaults.kernel_log->init();
    if (rc != 0)
        panic("Failed to initialize kernel log device: %d\n", rc);

    const char *systimer_compatible = "arm,armv7-timer";
    kprintf("Initializing system timer (%s)...\n", systimer_compatible);
    Driver const *systimer_drv = find_driver(systimer_compatible);
    kassert(systimer_drv != nullptr);
    ARMv7Timer::Config systimer_config {
        .irq = 30,
    };
    auto *systimer = reinterpret_cast<SystemTimer*>(systimer_drv->load(systimer_compatible, mustmalloc(systimer_drv->required_space), &systimer_config));
    rc = systimer->init();
    if (rc != 0)
        panic("Failed to initialize system timer: %d\n", rc);
    register_device(systimer);
    s_defaults.systimer = systimer;

    const char *rtc_compatible = "arm,pl031";
    kprintf("Initializing RTC (%s)...\n", rtc_compatible);
    Driver const *rtc_drv = find_driver(rtc_compatible);
    kassert(rtc_drv != nullptr);
    PL031::Config rtc_config {
        .physaddr = 0x09010000
    };
    auto *rtc = reinterpret_cast<RealTimeClock*>(rtc_drv->load(rtc_compatible, mustmalloc(rtc_drv->required_space), &rtc_config));
    rc = rtc->init();
    if (rc != 0)
        panic("Failed to initialize RTC: %d\n", rc);
    register_device(rtc);
    api::DateTime now;
    if (0 == rtc->get_time(now)) {
        kprintf("Current time: %d-%d-%d %d:%d:%d\n", now.year, now.month, now.day, now.hour, now.minute, now.second);
    }

    constexpr uintptr_t VIRTIO_MMIO_FIRST_ADDR = 0xa000000;
    constexpr uint32_t VIRTIO_MMIO_FIRST_IRQ = 0x10;
    for (int32_t i = 0; i < 30; i++) {
        uintptr_t virtio_mmio_addr = VIRTIO_MMIO_FIRST_ADDR + 0x200 * i;
        uint32_t virtio_irq = 32 + VIRTIO_MMIO_FIRST_IRQ + i;

        switch (virtio_util_probe(virtio_mmio_addr)) {
            case VirtioDeviceID::BlockDevice: {
                Driver const *virtio_drv = find_driver("virtioblk,mmio");
                kassert(virtio_drv != nullptr);
    
                auto config = VirtioBlockDevice::Config {
                    .address = virtio_mmio_addr,
                    .irq = virtio_irq
                };
                kprintf("Initializing %s device @ %p...\n", virtio_drv->compatible, virtio_mmio_addr);
                auto *virtio_mmio_dev = reinterpret_cast<VirtioBlockDevice*>(
                    virtio_drv->load(virtio_drv->compatible, mustmalloc(virtio_drv->required_space), &config)
                );
                rc = virtio_mmio_dev->init();
                if (rc != 0)
                    panic("Failed to initialize %s device @ %p: %d\n", virtio_drv->compatible, virtio_mmio_addr, rc);
        
                register_device(virtio_mmio_dev);
                if (s_defaults.storage == nullptr)
                    s_defaults.storage = virtio_mmio_dev;

                break;
            }

            case VirtioDeviceID::GPU: {
                Driver const *virtio_drv = find_driver("virtiogpu,mmio");
                kassert(virtio_drv != nullptr);
    
                auto config = VirtioGPU::Config {
                    .address = virtio_mmio_addr,
                    .irq = virtio_irq
                };
                kprintf("Initializing %s device @ %p...\n", virtio_drv->compatible, virtio_mmio_addr);
                auto *virtio_mmio_dev = reinterpret_cast<VirtioGPU*>(
                    virtio_drv->load(virtio_drv->compatible, mustmalloc(virtio_drv->required_space), &config)
                );
                rc = virtio_mmio_dev->init();
                if (rc != 0)
                    panic("Failed to initialize %s device @ %p: %d\n", virtio_drv->compatible, virtio_mmio_addr, rc);

                register_device(virtio_mmio_dev);
                break;
            }

            case VirtioDeviceID::InputDevice: {
                Driver const *virtio_drv = find_driver("virtioinput,mmio");
                kassert(virtio_drv != nullptr);
    
                auto config = VirtioInputDevice::Config {
                    .address = virtio_mmio_addr,
                    .irq = virtio_irq
                };
                kprintf("Initializing %s device @ %p...\n", virtio_drv->compatible, virtio_mmio_addr);
                auto *virtio_mmio_dev = reinterpret_cast<VirtioInputDevice*>(
                    virtio_drv->load(virtio_drv->compatible, mustmalloc(virtio_drv->required_space), &config)
                );
                rc = virtio_mmio_dev->init();
                if (rc != 0)
                    panic("Failed to initialize %s device @ %p: %d\n", virtio_drv->compatible, virtio_mmio_addr, rc);
        
                register_device(virtio_mmio_dev);
                break;
            }

            case VirtioDeviceID::Unsupported:
            case VirtioDeviceID::Invalid:
            default:
                continue;
        }

    }
}
