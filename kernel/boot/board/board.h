#pragma once

#include <stdint.h>
#include <stddef.h>


void board_early_console_init(void);

void board_early_putchar(char c);

typedef struct {
    uintptr_t start;
    size_t size;
} range_t;

range_t board_early_get_ram_range();

range_t board_early_get_bootmem_range();

extern "C"
{

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *s, int c, size_t n);

}