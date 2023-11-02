#include <stdlib.h>
#include "libsstring.h"


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
