#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
} log_level_t;

int  logger_init(log_level_t level);

void logger_finalize(void);

void log_error(const char *fmt, ...);

void log_info (const char *fmt, ...);

void log_debug(const char *fmt, ...);

void log_warn(const char *fmt, ...);

#endif