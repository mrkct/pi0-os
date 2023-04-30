#pragma once

#include <kernel/error.h>
#include <stdarg.h>
#include <stddef.h>

namespace kernel {

struct VideoConsole;

void kprintf_video_init(VideoConsole&);

size_t ksnprintf(char* buffer, size_t buffer_size, char const* format, va_list args);

size_t kprintf(char const* format, ...);

}
