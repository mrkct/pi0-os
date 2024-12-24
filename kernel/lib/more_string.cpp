#include <kernel/base.h>
#include "more_string.h"


extern "C" size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (*s++ && len < maxlen)
        len++;

    return len;
}

extern "C" int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int diff = tolower(*s1) - tolower(*s2);
        if (diff != 0)
            return diff;
        s1++;
        s2++;
    }

    return tolower(*s1) - tolower(*s2);
}

extern "C" int strncasecmp(const char s1[], const char s2[], size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char c1 = tolower(s1[i]);
        char c2 = tolower(s2[i]);

        if (c1 != c2)
            return c1 - c2;

        if (c1 == '\0')
            return 0;
    }

    return 0;
}

extern "C" char *strdup(const char *s)
{
    size_t len = strlen(s);
    char *dup = (char*) malloc(len + 1);
    if (dup == nullptr)
        return nullptr;
    memcpy(dup, s, len + 1);
    return dup;
}
