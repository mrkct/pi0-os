#include <kernel/base.h>
#include "devicemanager.h"

#include "char/pl011.h"
#include "char/console/chardevconsole.h"


static Device *s_devices[64];
static struct {
    CharacterDevice *kernel_log;
    BlockDevice *storage;
} s_defaults;
static uint8_t s_earlycon_storage[256]; 

struct Driver {
    const char *compatible;
    size_t required_space;
    Device *(*load)(const char *compatible, void *storage, uint32_t arg);
};

static constexpr Driver s_drivers[] = {
    {
        .compatible = "arm,pl011",
        .required_space = sizeof(PL011UART),
        .load = [](const char*, void *storage, uint32_t arg) -> Device* { return new (storage) PL011UART(arg); }
    }
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
    if (boot_params->ram_start == 0x8000) {
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
    case DetectedMachine::Raspi0:
        todo();
        break;
    case DetectedMachine::Virt: {
        console = (CharacterDevice*) find_driver("arm,pl011")->load("arm,pl011", s_earlycon_storage, 0x9000000);
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
