#include <kernel/lib/libc/string.h>

extern "C" void* memset(void* s, int c, size_t n)
{
    unsigned char* p = (unsigned char*)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

extern "C" void* memcpy(void* dest, void const* src, size_t n)
{
    unsigned char* d = (unsigned char*)dest;
    unsigned char const* s = (unsigned char const*)src;
    while (n--)
        *d++ = *s++;
    return dest;
}
