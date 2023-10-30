#pragma once 

#include <kernel/error.h>
#include <kernel/memory/vm.h>


namespace kernel {

Error try_load_elf(uint8_t const *elf_binary, size_t binary_size, AddressSpace&, uintptr_t &entry_point, bool is_kernel_task);

}
