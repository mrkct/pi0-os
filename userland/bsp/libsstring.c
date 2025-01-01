#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libsstring.h"


bool string_startswith(const char *string, const char *prefix)
{
    while (*string && *prefix && *string == *prefix) {
        string++;
        prefix++;
    }

    return *prefix == '\0'; 
}

size_t string_split(const char *string, const char *divider, Range **ranges)
{
    if (string == NULL || divider == NULL)
        return 0;

    const size_t dividers_length = strlen(divider);

    size_t ranges_length = 0;
    *ranges = NULL;

    const char *current = string;
    size_t start_index = 0;
    
    while (*current) {
        while (*current && !string_startswith(current, divider)) {
            current++;
        }
        *ranges = realloc(*ranges, ranges_length);
        (*ranges)[ranges_length] = (Range){.start = start_index, .length = (current - string) - start_index};
        ranges_length += 1;
        start_index = current - string;
        current += dividers_length;
    }

    return ranges_length;
}

void print_string_range(const char *str, Range range)
{
    for (size_t i = 0; i < range.length; i++)
        putchar(str[range.start + i]);
}
