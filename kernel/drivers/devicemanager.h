#pragma once

#include "device.h"

void devicemanager_init_kernel_log_device(BootParams const *boot_params);

void devicemanager_load_available_peripherals(BootParams const *boot_params);

Device *devicemanager_get_by_name(const char *name);

Device *devicemanager_get_by_devid(uint16_t devid);

CharacterDevice *devicemanager_get_kernel_log_device();

InterruptController *devicemanager_get_interrupt_controller_device();

SystemTimer *devicemanager_get_system_timer_device();

BlockDevice *devicemanager_get_root_block_device();
