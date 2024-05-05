#include <kernel/base.h>
#include "more_string.h"


extern "C" size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (*s++ && len < maxlen)
        len++;

    return len;
}
