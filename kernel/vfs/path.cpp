#include "path.h"

static char to_lower(char c)
{
    return 'A' <= c && c <= 'Z' ? 'a' + (c - 'A') : c;
}

static const char *strrchr(Path path, char c)
{
    for (int i = (int) path.len - 1; i >= 0; i--) {
        if (path.str[i] == c)
            return &path.str[i];
    }
    return NULL;
}

bool path_startswith(Path path, Path prefix)
{
    if (path.len < prefix.len)
        return false;
    
    for (uint32_t i = 0; i < prefix.len; i++) {
        if (path.str[i] != prefix.str[i])
            return false;
    }
    
    return true;
}

bool path_compare(Path a, Path b, bool case_sensitive)
{
    if (a.len != b.len)
        return false;

    const auto &pathcmp = [](Path a, Path b, bool (*cmp)(char, char)) {
        int i;
        for (i = 0; i < a.len && i < b.len; i++) {
            if (!cmp(a.str[i], b.str[i]))
                return false;
        }
        return true;
    };

    if (case_sensitive) {
        return pathcmp(a, b, [](char a, char b) { return a == b; });
    } else {
        return pathcmp(a, b, [](char a, char b) { return to_lower(a) == to_lower(b); });
    }
}

bool path_compare(Path a, const char *b, bool case_sensitive)
{
    return path_compare(a, path_from_string(b), case_sensitive);
}

Path path_basedir(Path a)
{
    const char *last_separator = strrchr(a, '/');
    if (last_separator == NULL)
        return path_from_string("");
    
    return Path {
        .str = a.str,
        .len = (uint32_t)(last_separator - a.str)
    };
}

Path path_basename(Path a)
{
    const char *last_separator = strrchr(a, '/');
    if (last_separator == NULL)
        return a;
    
    // Skip the '/'
    last_separator += 1;
    return Path {
        .str = last_separator,
        .len = a.len - (last_separator - a.str)
    };
}
