#include <stdio.h>
#include "misc.h"
#include "board/board.h"


static struct {
    uintptr_t start;
    uintptr_t end;
    uintptr_t current;
} s_bootmem;

void bootmem_init(uintptr_t start, uintptr_t size)
{
    s_bootmem.start = start;
    s_bootmem.end = start + size;
    s_bootmem.current = start;
}

void *bootmem_alloc(size_t size, size_t alignment)
{
    uintptr_t aligned = s_bootmem.current;
    aligned = (aligned + alignment - 1) & ~(alignment - 1);
    EARLY_ASSERT(aligned + size <= s_bootmem.end);
    s_bootmem.current = aligned + size;
    return (void *) aligned;
}

size_t bootmem_allocated(void)
{
    return s_bootmem.current - s_bootmem.start;
}

static bool represent_integer(char* buffer, size_t* written, size_t buffer_size, uint64_t num, int base, int pad_to)
{
    if (num == 0) {
        if (*written >= buffer_size - 1)
            return false;

        buffer[(*written)++] = '0';
        return true;
    }

    int num_digits = 0;
    for (uint64_t n = num; n > 0; n /= base)
        num_digits++;

    if (pad_to > num_digits)
        num_digits = pad_to;

    if (*written + num_digits >= buffer_size - 1)
        return false;

    for (int i = num_digits - 1; i >= 0; i--) {
        int digit = num % base;
        if (digit < 10)
            buffer[*written + i] = '0' + digit;
        else
            buffer[*written + i] = 'a' + digit - 10;
        num /= base;
    }

    *written += num_digits;
    return true;
}

static size_t mini_snprintf(char* buffer, size_t buffer_size, char const* format, va_list args)
{
#define APPEND(c)                              \
    do {                                       \
        if (written >= buffer_size - 1)        \
            goto append_terminator_and_return; \
        buffer[written++] = (c);               \
    } while (0)

    if (buffer_size == 0)
        return 0;

    size_t written = 0;
    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            APPEND(format[i]);
            continue;
        }

        switch (format[++i]) {
        case 's': {
            char const* str = va_arg(args, char const*);
            for (size_t j = 0; str[j] != '\0'; j++)
                APPEND(str[j]);
            break;
        }

        case 'd': {
            int num = va_arg(args, int);
            if (num < 0) {
                APPEND('-');
                num = -num;
            }
            if (!represent_integer(buffer, &written, buffer_size, (uint64_t)num, 10, 0))
                goto append_terminator_and_return;
            break;
        }

        case 'u': {
            if (!represent_integer(buffer, &written, buffer_size, va_arg(args, unsigned), 10, 0))
                goto append_terminator_and_return;
            break;
        }

        case 'p': {
            APPEND('0');
            APPEND('x');
            if (!represent_integer(buffer, &written, buffer_size, va_arg(args, unsigned), 16, 8))
                goto append_terminator_and_return;

            break;
        }

        default: {
            APPEND('%');
            APPEND(format[i]);
            break;
        }
        }
    }

#undef APPEND

append_terminator_and_return:
    buffer[written] = '\0';
    return written;
}

size_t early_kprintf(char const* format, ...)
{
    static char buffer[1024];

    va_list args;
    va_start(args, format);
    size_t written = mini_snprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    for (size_t i = 0; i < written; i++) {
        board_early_putchar(buffer[i]);
    }

    return written;
}

#define UNALIGNED(s) (((uintptr_t) (s)) & 3)

extern "C" {

void *memcpy(void *dest, const void *source, size_t n)
{
    uint8_t *src = (uint8_t*) source;
    uint8_t *dst = (uint8_t*) dest;
    if (n >= 4 && !(UNALIGNED(dst) || UNALIGNED(src))) {
        uint32_t *aligned_dst = (uint32_t*) dst;
        uint32_t *aligned_src = (uint32_t*) src;
        while (n >= 16) {
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            n -= 16;
        }
        
        while (n >= 4) {
            *aligned_dst++ = *aligned_src++;
            n -= 4;
        }
        dst = (uint8_t*) aligned_dst;
        src = (uint8_t*) aligned_src;
    }
    
    while (n--)
        *dst++ = *src++;

    return dest;
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t*)s;
    while (n && UNALIGNED(p)) {
        *p++ = (uint8_t) c;
        n--;
    }

    if (n >= 4) {
        uint32_t c32 = c | (c << 8) | (c << 16) | (c << 24);
        uint32_t *p32 = (uint32_t*) p;
        while (n >= 4) {
            *p32++ = c32;
            n -= 4;
        }
        p = (uint8_t*)p32;
    }

    while (n) {
        *p++ = (uint8_t)c;
        n--;
    }

    return s;
}

}
