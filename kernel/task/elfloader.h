#pragma once

#include <kernel/error.h>
#include <kernel/memory/vm.h>


int elf_load_into_address_space(const char *path, uintptr_t *entrypoint, AddressSpace &as);
