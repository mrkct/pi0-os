#include <kernel/memory/bootalloc.h>

#include "devicemanager.h"
#include "char/pl011.h"
#include "char/bcm2835_aux_uart.h"
#include "char/bcm2835_gpio.h"
#include "char/console/chardevconsole.h"


static Device *s_devices[64];
static struct {
    CharacterDevice *kernel_log;
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


static void raspi0_load_peripherals()
{
    todo();
}

static void virt_load_peripherals()
{
    // TODO: The other peripherals...
}

void devicemanager_init_kernel_log_device(BootParams const *boot_params)
{
    CharacterDevice *console = nullptr;

    switch (detect_machine(boot_params)) {
    case DetectedMachine::Raspi0: {
        const char *gpio_compatible = "brcm,bcm2835-gpio";
        Driver const *gpio_drv = find_driver(gpio_compatible);
        kassert(gpio_drv != nullptr);

        const uintptr_t iobase = 0x20000000;

        BCM2835GPIOController::Config gpio_config {
            .iobase = iobase,
            .offset = 0x00200000,   // FIXME: Check this
        };
        GPIOController *gpio_dev = reinterpret_cast<GPIOController*>(gpio_drv->load(gpio_compatible, bootalloc(gpio_drv->required_space), &gpio_config));
        if (gpio_dev && 0 == gpio_dev->init()) {
            register_device(gpio_dev);
            
            gpio_dev->configure_pin_pull_up_down(0, 14, GPIOController::PullState::None);
            gpio_dev->configure_pin(0, 14, GPIOController::PinFunction::Alt5);

            gpio_dev->configure_pin_pull_up_down(0, 15, GPIOController::PullState::None);
            gpio_dev->configure_pin(0, 15, GPIOController::PinFunction::Alt5);


            const char *uart_compatible = "brcm,bcm2835-aux-uart";
            Driver const *uart_drv = find_driver(uart_compatible);
            kassert(uart_drv != nullptr);
        
            BCM2835AuxUART::Config uart_config {
                .iobase = iobase,
                .offset = 0x215000,
            };
            Device *uart_dev = uart_drv->load(uart_compatible, bootalloc(uart_drv->required_space), &uart_config);
            if (uart_dev && 0 == uart_dev->init()) {
                register_device(uart_dev);
                s_defaults.kernel_log = reinterpret_cast<CharacterDevice*>(uart_dev);
            }
        }
        
        break;
    }
    case DetectedMachine::Virt: {
        Driver const *driver = find_driver("arm,pl011");
        PL011UART::Config config {
            .physaddr = 0x9000000
        };
        console = (CharacterDevice*) driver->load("arm,pl011", bootalloc(driver->required_space), &config);
        if (console && 0 == console->init()) {
            register_device(console);
            s_defaults.kernel_log = console;
        }
        break;
    }
    case DetectedMachine::Unknown:
        panic("Unknown machine");
    }
}

void devicemanager_init_available_peripherals(BootParams const *boot_params)
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

    for (size_t i = 0; i < array_size(s_devices); i++) {
        if (s_devices[i] == nullptr) {
            break;
        }
        s_devices[i]->init();
    }
}

CharacterDevice *devicemanager_get_kernel_log_device() { return s_defaults.kernel_log; }
