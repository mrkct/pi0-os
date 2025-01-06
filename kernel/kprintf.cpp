#include <stdio.h>
#include <kernel/kprintf.h>
#include <kernel/locking/irqlock.h>


static PutsFunc s_puts = NULL;


void kprintf_set_puts_func(PutsFunc f)
{
    s_puts = f;
}

size_t kprintf(char const* format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[1024];
    size_t written = vsnprintf(buffer, sizeof(buffer), format, args);

    if (s_puts) {
        s_puts(buffer, written);
    }

    va_end(args);
    return written;
}
