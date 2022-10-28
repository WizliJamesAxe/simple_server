#include <stdarg.h>
#include <stdio.h>

#include "log.h"

void _log_(
	const char * func,
	int line,
	int level,
	const char * format, ...)
{
	if (!format)
		return;

	char buf[1024];

	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	if (LOG_ERR == level)
		fprintf(stderr, "<%s:%d> %s\n", func, line, buf);
	else
		printf("<%s:%d> %s\n", func, line, buf);
	// syslog(level, "<%s:%d> %s", func, line, buf);
}
