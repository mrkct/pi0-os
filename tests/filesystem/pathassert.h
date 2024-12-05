#pragma once

#include <kernel/vfs/path.h>

static inline void ASSERT_EQUAL_PATH(const char *expected, PathSlice const& actual)
{
    if (actual.compare(expected) != 0) {
        fprintf(stderr, "Assertion failed: Expected \"%s\", got \"", expected);
        for (uint32_t i = 0; i < actual.length(); i++) {
            fprintf(stderr, "%c", actual.path->str[actual.start + i]);
        }
        fprintf(stderr, "\"\n");
        exit(EXIT_FAILURE);
    }
}
