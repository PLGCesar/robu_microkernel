#ifndef ROBU_LIBC_SYSLOG_H
#define ROBU_LIBC_SYSLOG_H
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#define LOG_PID    0x01
#define LOG_USER   (1 << 3)
#define LOG_DAEMON (3 << 3)
#define LOG_AUTH   (4 << 3)
#include <stdarg.h>
void openlog(const char *ident, int option, int facility);
void syslog(int priority, const char *format, ...);
void vsyslog(int priority, const char *format, va_list ap);
void closelog(void);
#endif
