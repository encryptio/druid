#ifndef __LOGGER_H__
#define __LOGGER_H__

#define LOG_JUNK 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERR  3

#define LOG_ALL LOG_JUNK
#define LOG_NONE (LOG_ERR+1)

#define LOG_DEFAULT LOG_INFO

void logger(int level, const char *module, const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));
void logger_set_output(int fd);
void logger_set_level(int level);

#endif
