#pragma once

#include "device.h"

void devicemanager_init_kernel_log_device(BootParams const *boot_params);

void devicemanager_init_available_peripherals(BootParams const *boot_params);

CharacterDevice *devicemanager_get_kernel_log_device();

InterruptController *devicemanager_get_interrupt_controller_device();
