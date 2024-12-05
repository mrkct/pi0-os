#include "bootalloc.h"


static uint8_t s_bootalloc_storage[1024];
static uint8_t *s_bootalloc_next = s_bootalloc_storage;

void *bootalloc(size_t size)
{
    if (s_bootalloc_next + size > s_bootalloc_storage + sizeof(s_bootalloc_storage))
        panic("Out of boot memory");
    
    auto ret = s_bootalloc_next;
    s_bootalloc_next += round_up<size_t>(size, 16);
    return ret;
}
