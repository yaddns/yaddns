#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>

#include "log.h"

void log_open(void)
{
#ifdef ENABLE_SYSLOG
	openlog("yaddns", LOG_CONS, LOG_DAEMON);
#endif
}

void log_close(void)
{
#ifdef ENABLE_SYSLOG
	closelog();
#endif
}

void log_it(int priority, char const *format, ...)
{
	va_list ap;

	va_start(ap, format);
#ifdef ENABLE_SYSLOG
	vsyslog(priority, format, ap);
#else
	(void)(priority);
	vprintf(format, ap);
#endif
	va_end(ap);
}
