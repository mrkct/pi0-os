#pragma once

#include <kernel/error.h>

namespace kernel {

Error kheap_init();

Error _kmalloc(size_t size, uintptr_t& address);
Error _kfree(uintptr_t address);

template<typename T>
Error kmalloc(size_t size, T*& address)
{
    uintptr_t addr;
    TRY(_kmalloc(size, addr));
    address = reinterpret_cast<T*>(addr);
    return Success;
}

template<typename T>
Error kfree(T* address)
{
    return _kfree(reinterpret_cast<uintptr_t>(address));
}

}
