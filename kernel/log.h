#pragma once

#include <kernel/kprintf.h>


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#ifdef LOG_ENABLED

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

#ifdef UNIT_TEST
#define get_ticks_ms() (uint64_t)0
#else
extern uint32_t get_ticks_ms();
#endif

#define LOGD(format, ...) kprintf("% 8" PRIu32 " - " LOG_TAG ANSI_COLOR_MAGENTA "DBUG: " format ANSI_COLOR_RESET "\n",get_ticks_ms(), ## __VA_ARGS__)
#define LOGI(format, ...) kprintf("% 8" PRIu32 " - " LOG_TAG ANSI_COLOR_GREEN "INFO: " format ANSI_COLOR_RESET "\n",  get_ticks_ms(), ## __VA_ARGS__)
#define LOGW(format, ...) kprintf("% 8" PRIu32 " - " LOG_TAG ANSI_COLOR_YELLOW "WARN: " format ANSI_COLOR_RESET "\n", get_ticks_ms(), ## __VA_ARGS__)
#define LOGE(format, ...) kprintf("% 8" PRIu32 " - " LOG_TAG ANSI_COLOR_RED "ERR:  " format ANSI_COLOR_RESET "\n",    get_ticks_ms(), ## __VA_ARGS__)

#else

#define LOGD(format, ...) do_nothing(format, ## __VA_ARGS__)
#define LOGI(format, ...) do_nothing(format, ## __VA_ARGS__)
#define LOGW(format, ...) do_nothing(format, ## __VA_ARGS__)
#define LOGE(format, ...) do_nothing(format, ## __VA_ARGS__)

static inline void do_nothing(const char* format, ...)
{
    (void) format;
}

#endif
