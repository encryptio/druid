#include "logger.h"

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

static int log_level = LOG_DEFAULT;
static int log_fd = 2; // stderr

#define LOG_BUF_SIZE 256

static char *log_level_message(int level) {
    switch ( level ) {
        case LOG_JUNK: return "junk";
        case LOG_INFO: return "info";
        case LOG_WARN: return "warn";
        case LOG_ERR:  return "error";
        default: return "unknown";
    }
}

void logger(int level, const char *module, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    char buf[LOG_BUF_SIZE];
    int at = 0;

    if ( level < log_level )
        return;

    at += snprintf(buf+at, LOG_BUF_SIZE-at, "[%s] (%s) ", module, log_level_message(level));
    at += vsnprintf(buf+at, LOG_BUF_SIZE-at, format, ap);
    at += snprintf(buf+at, LOG_BUF_SIZE-at, "\n");

    // ignore write errors, there's nothing we can do
    write(log_fd, buf, at);
}

void logger_set_output(int fd) {
    // TODO: is there some way to verify the fd?
    log_fd = fd;
}

void logger_set_level(int level) {
    if ( level >= LOG_ALL && level <= LOG_NONE )
        log_level = level;
}

