#include <kernel/lib/string.h>

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

char* strncpy_safe(char* dest, char const* src, size_t n)
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

int strcmp(char const* s1, char const* s2)
{
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

size_t strlen(char const* s)
{
    size_t len = 0;
    while (*s++)
        len++;

    return len;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (*s++ && len < maxlen)
        len++;

    return len;
}
