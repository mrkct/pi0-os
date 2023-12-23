#include <kernel/device/miniuart.h>
#include <kernel/kprintf.h>
#include <kernel/lib/math.h>
#include <kernel/locking/reentrant.h>


namespace kernel {

static ReentrantSpinlock g_kprintf_lock = REENTRANT_SPINLOCK_START;
static VideoConsole* g_video_console;

void kprintf_video_init(VideoConsole& video_console)
{
    g_video_console = &video_console;
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

size_t ksnprintf(char* buffer, size_t buffer_size, char const* format, va_list args)
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
        case 'c': {
            char c = (char)va_arg(args, int);
            APPEND(c);
            break;
        }

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

        case 'l': {
            // FIXME: "long" is actually 4 bytes on arm6, yet here we treat it as 8 bytes
            char next = format[++i]; // FIXME: Bound checking

            if (next == 'd') {
                long num = va_arg(args, int64_t);
                if (num < 0) {
                    APPEND('-');
                    num = -num;
                }
                if (!represent_integer(buffer, &written, buffer_size, (uint64_t)num, 10, 0))
                    goto append_terminator_and_return;
            } else if (next == 'u') {
                if (!represent_integer(buffer, &written, buffer_size, va_arg(args, uint64_t), 10, 0))
                    goto append_terminator_and_return;
            } else if (next == 'x') {
                if (!represent_integer(buffer, &written, buffer_size, va_arg(args, uint64_t), 16, 8))
                    goto append_terminator_and_return;
            } else {
                APPEND('%');
                APPEND('l');
                APPEND(next);
            }
            break;
        }

        case 'b': {
            if (!represent_integer(buffer, &written, buffer_size, va_arg(args, unsigned), 2, 8))
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

        case 'x': {
            if (!represent_integer(buffer, &written, buffer_size, va_arg(args, unsigned), 16, 8))
                goto append_terminator_and_return;

            break;
        }

        case '%': {
            APPEND('%');
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

size_t ksprintf(char* buffer, size_t buffer_size, char const* format, ...)
{
    va_list args;
    va_start(args, format);
    size_t written = ksnprintf(buffer, buffer_size, format, args);
    va_end(args);

    return written;
}

size_t kprintf(char const* format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[1024];
    size_t written = ksnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    take(g_kprintf_lock);
    for (size_t i = 0; i < written; i++) {
        miniuart_putc((unsigned char)buffer[i]);
    }
    release(g_kprintf_lock);

    return written;
}

}
