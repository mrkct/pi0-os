#pragma once

#include <kernel/base.h>
#include <stdlib.h>
#include <kernel/error.h>


Error kheap_init();

static inline void *mustmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == nullptr) {
        panic("malloc failed\n");
    }
    return ptr;
}

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

Error krealloc(void*&, size_t size);
