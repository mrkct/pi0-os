#include <kernel/lib/memory.h>
#include <stdint.h>

namespace klib {

void kmemcpy(void* dest, void const* src, size_t n)
{
    for (size_t i = 0; i < n; i++)
        ((uint8_t*)dest)[i] = ((uint8_t*)src)[i];
}

void kmemset(void* s, uint8_t c, size_t n)
{
    for (size_t i = 0; i < n; i++)
        ((uint8_t*)s)[i] = c;
}

}
