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

}
