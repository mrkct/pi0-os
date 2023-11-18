#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>


typedef struct Range {
    size_t start, length;
} Range;

bool string_startswith(const char *string, const char *prefix);

size_t string_split(const char *string, const char *divider, Range **ranges);

int string_compare_ranges(const char *a, Range a_range, const char *b, Range b_range);

int string_compare_to_range(const char *a, const char *b, Range b_range);

static inline void free_ranges(Range *ranges) { free(ranges); }

void print_string_range(const char *str, Range range);
