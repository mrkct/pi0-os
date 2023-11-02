#pragma once

#include <stdlib.h>


char *read_line(const char *prompt);

static inline void free_line(char *line) { free(line); }
