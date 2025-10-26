/*
 * Syslog facility and priority name tables
 *
 * Provides CODE structure and name-to-value mappings for syslog priorities
 * on systems that don't provide SYSLOG_NAMES (e.g., Linux/glibc).
 */

#ifndef _SYSLOG_NAMES_H
#define _SYSLOG_NAMES_H

#include <syslog.h>

/* Define CODE structure if not provided by system */
#ifndef CODE
typedef struct _code {
	const char *c_name;
	int	    c_val;
} CODE;
#endif

/* Syslog facility names for priority parsing */
static CODE facilitynames[] = { { "auth", LOG_AUTH },
#ifdef LOG_AUTHPRIV
	{ "authpriv", LOG_AUTHPRIV },
#endif
	{ "cron", LOG_CRON }, { "daemon", LOG_DAEMON },
#ifdef LOG_FTP
	{ "ftp", LOG_FTP },
#endif
	{ "kern", LOG_KERN }, { "lpr", LOG_LPR }, { "mail", LOG_MAIL }, { "news", LOG_NEWS },
	{ "syslog", LOG_SYSLOG }, { "user", LOG_USER }, { "uucp", LOG_UUCP },
	{ "local0", LOG_LOCAL0 }, { "local1", LOG_LOCAL1 }, { "local2", LOG_LOCAL2 },
	{ "local3", LOG_LOCAL3 }, { "local4", LOG_LOCAL4 }, { "local5", LOG_LOCAL5 },
	{ "local6", LOG_LOCAL6 }, { "local7", LOG_LOCAL7 }, { NULL, -1 } };

/* Syslog priority names for priority parsing */
static CODE prioritynames[] = { { "alert", LOG_ALERT }, { "crit", LOG_CRIT },
	{ "debug", LOG_DEBUG }, { "emerg", LOG_EMERG }, { "err", LOG_ERR }, { "error", LOG_ERR },
	{ "info", LOG_INFO }, { "notice", LOG_NOTICE }, { "panic", LOG_EMERG },
	{ "warn", LOG_WARNING }, { "warning", LOG_WARNING }, { NULL, -1 } };

#endif /* _SYSLOG_NAMES_H */
