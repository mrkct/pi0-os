#include <kernel/lib/string.h>

namespace klib {

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

}
