#include <kernel/lib/string.h>


#define UNALIGNED(s) (((uintptr_t) (s)) & 3)


extern "C" void* memset(void* s, int c, size_t n)
{
    unsigned char* p = (unsigned char*)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

extern "C" void* memcpy(void* dest, void const* source, size_t n)
{
    uint8_t *src = (uint8_t*) source;
    uint8_t *dst = (uint8_t*) dest;

    if (n >= 4 && !(UNALIGNED(dst) || UNALIGNED(src))) {
        uint32_t *aligned_dst = (uint32_t*) dst;
        uint32_t *aligned_src = (uint32_t*) src;

        while (n >= 16) {
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            n -= 16;
        }
        
        while (n >= 4) {
            *aligned_dst++ = *aligned_src++;
            n -= 4;
        }

        dst = (uint8_t*) aligned_dst;
        src = (uint8_t*) aligned_src;
    }
    
    while (n--)
        *dst++ = *src++;

    return dest;
}

extern "C" int memcmp(const void *s1, const void *s2, size_t n)
{
    const int8_t *a = (int8_t const*) s1;
    const int8_t *b = (int8_t const*) s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return a[i] - b[i];
    }
    
    return 0;
}

extern "C" char *strcpy(char *dst, const char *src)
{
    char *ret_dst = dst;
    while (*src) {
        *dst = *src;
        dst++;
        src++;
    }
    *dst = '\0';

    return ret_dst;
}

extern "C" char* strncpy_safe(char* dest, char const* src, size_t n)
{
    if (n == 0)
        return dest;

    size_t i;
    for (i = 0; i < n - 1 && *src; i++, src++) {
        *dest++ = *src;
    }
    *dest = '\0';

    return dest;
}

extern "C" int strcmp(char const* s1, char const* s2)
{
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

extern "C" size_t strlen(char const* s)
{
    size_t len = 0;
    while (*s++)
        len++;

    return len;
}

extern "C" size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (*s++ && len < maxlen)
        len++;

    return len;
}
