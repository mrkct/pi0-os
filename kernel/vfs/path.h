#pragma once

#include <kernel/base.h>

struct Path {
    const char *str;
    uint32_t len;
};

static inline Path path_from_string(const char *s)
{
    return Path{
        .str = s,
        .len = strlen(s)
    };
}

bool path_startswith(Path path, Path prefix);

bool path_compare(Path a, Path b, bool case_sensitive);

bool path_compare(Path a, const char *b, bool case_sensitive);

Path path_basedir(Path a);

Path path_basename(Path a);
