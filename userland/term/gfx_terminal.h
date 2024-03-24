#pragma once

#include <stdlib.h>
#include "libgfx.h"


#define COL_BACKGROUND COL_BLUE
#define COL_FOREGROUND COL_WHITE

void gfx_terminal_init(void);

void gfx_terminal_print(const char *s, uint32_t background_color, uint32_t foreground_color);
