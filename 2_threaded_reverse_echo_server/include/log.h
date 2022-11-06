#ifndef _MY_LOG_H_
#define _MY_LOG_H_
#include <syslog.h>

void _log_(
	const char * func,
	int line,
	int level,
	const char * format, ...
) __attribute__((format(printf, 4, 5)));

#define log_ERROR(...) \
	_log_(__func__, __LINE__, LOG_ERR, __VA_ARGS__)

#define log_DEBUG(...) \
	_log_(__func__, __LINE__, LOG_DEBUG, __VA_ARGS__)

#endif // _MY_LOG_H_
