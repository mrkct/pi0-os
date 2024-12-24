#include <kernel/base.h>


extern "C" size_t strnlen(const char *s, size_t maxlen);

extern "C" int strcasecmp(const char *s1, const char *s2);

extern "C" int strncasecmp(const char s1[], const char s2[], size_t n);

extern "C" char *strdup(const char *s);

static inline bool startswith(const char *s, const char *prefix)
{
    return 0 == strncmp(s, prefix, strlen(prefix));
}
