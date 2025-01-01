#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct Range {
    size_t start, length;
} Range;

bool string_startswith(char const* string, char const* prefix);

size_t string_split(char const* string, char const* divider, Range** ranges);

int string_compare_ranges(char const* a, Range a_range, char const* b, Range b_range);

int string_compare_to_range(char const* a, char const* b, Range b_range);

static inline void free_ranges(Range* ranges) { free(ranges); }

void print_string_range(char const* str, Range range);
