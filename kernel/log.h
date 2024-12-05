#pragma once

#include <kernel/kprintf.h>


#ifndef LOG_TAG
#error You must defined LOG_TAG before #include <kernel/log.h>
#endif

#ifdef UNIT_TEST
#define get_ticks_ms() (uint64_t)0
#else
extern uint32_t get_ticks_ms();
#endif

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"


#define LOGW(format, ...) kprintf("% 8" PRIu32 " - [% 8s] " ANSI_COLOR_YELLOW  "WARN: " format ANSI_COLOR_RESET "\n", get_ticks_ms(), LOG_TAG, ## __VA_ARGS__)
#define LOGE(format, ...) kprintf("% 8" PRIu32 " - [% 8s] " ANSI_COLOR_RED     "ERR:  " format ANSI_COLOR_RESET "\n", get_ticks_ms(), LOG_TAG, ## __VA_ARGS__)

#ifdef LOG_ENABLED

#define LOGD(format, ...) kprintf("% 8" PRIu32 " - [% 8s] " ANSI_COLOR_MAGENTA "DBUG: " format ANSI_COLOR_RESET "\n", get_ticks_ms(), LOG_TAG, ## __VA_ARGS__)
#define LOGI(format, ...) kprintf("% 8" PRIu32 " - [% 8s] " ANSI_COLOR_GREEN   "INFO: " format ANSI_COLOR_RESET "\n", get_ticks_ms(), LOG_TAG, ## __VA_ARGS__)

#else

static inline void do_nothing(const char* format, ...)
{
    (void) format;
}

#define LOGD(format, ...) do_nothing(format, ## __VA_ARGS__)
#define LOGI(format, ...) do_nothing(format, ## __VA_ARGS__)

#endif


