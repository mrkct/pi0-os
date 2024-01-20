#pragma once

#include <stdlib.h>

char* read_line(char const* prompt);

static inline void free_line(char* line) { free(line); }
