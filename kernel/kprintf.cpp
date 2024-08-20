#include <stdio.h>
#include <kernel/kprintf.h>
#include <kernel/locking/spinlock.h>


static Spinlock s_lock = SPINLOCK_START;
static PutCharFunc s_putchar = NULL;


void kprintf_set_putchar_func(PutCharFunc f)
{
    s_putchar = f;
}

size_t kprintf(char const* format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[1024];
    size_t written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (s_putchar) {
        take(s_lock);
        for (size_t i = 0; i < written; i++) {
            s_putchar((unsigned char) buffer[i]);
        }
        release(s_lock);
    }

    return written;
}
