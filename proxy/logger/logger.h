#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>

typedef enum {
    LOG_ERROR = 0,
    LOG_INFO  = 1,
    LOG_DEBUG = 2,
    LOG_WARN = 3,
} log_level_t;

int  logger_init(log_level_t level);

void logger_finalize(void);

void log_error(const char *fmt, ...);

void log_info (const char *fmt, ...);

void log_debug(const char *fmt, ...);

void log_warn(const char *fmt, ...);

#endif