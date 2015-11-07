
/*-
 * Copyright (c) 2015, Babak Farrokhi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "logfile.h"

#ifdef __linux__
#define _PROGNAME program_invocation_short_name
#else
#define _PROGNAME getprogname()
#endif					/* __linux__ */

/*
 * create/open given logfile and initialize appropriate struct
 */
log_t  *
log_open(const char *path, mode_t mode)
{
	log_t *lh;
	struct stat sb;
	char *filename;
	int fd;

	if (path == NULL) {
		asprintf(&filename, "%s/%s.log", LOGPATH, _PROGNAME);
	} else {
		filename = (char *)path;
	}

	/*
	 * try to create / append the file and make sure it is correctly
	 * created
	 */
	if ((fd = open(filename, O_WRONLY | O_APPEND | O_CREAT | O_SYNC, mode)) == -1)
		return (NULL);
	if (flock(fd, LOCK_EX) == -1) {
		close(fd);
		return (NULL);
	}
	if (stat(filename, &sb) == -1) {
		close(fd);
		return (NULL);
	}
	/* initialize data structure */
	lh = calloc(1, sizeof(log_t));

	lh->fd = fd;
	lh->dev = sb.st_dev;
	lh->ino = sb.st_ino;
	lh->mode = sb.st_mode;
	strncpy(lh->path, filename, strnlen(filename, MAXPATHLEN + 1));

	return (lh);
}

/*
 * close logfile handle and free up allocated struct
 */
void
log_close(const log_t *log)
{
	if (!log_isopen(log))
		return;

	close(log->fd);
	free((void *)log);
}

/*
 * check whether or not the logfile is opened
 */
inline	bool
log_isopen(const log_t *log)
{
	if (log == NULL)
		return (false);
	return (log->fd != -1);
}

/*
 * close and open logfile, used when a HUP signal is received (mosly in case of log roration)
 */
void
log_reopen(log_t **log)
{
	log_t *newlog;

	if (!log_isopen(*log))
		return;

	newlog = malloc(sizeof(log_t));
	memcpy(newlog, *log, sizeof(log_t));
	log_close(*log);
	*log = log_open(newlog->path, newlog->mode);
	free(newlog);
}

/*
 * printf given text into logfile
 */

void
log_printf(const log_t *log, const char *format,...)
{
	if (!log_isopen(log))
		return;

	va_list args;
	char *message;
	char *newline = "\n";

	va_start(args, format);
	vasprintf(&message, format, args);
	va_end(args);

	write(log->fd, message, strnlen(message, MAX_MSG_SIZE));
	write(log->fd, newline, sizeof(*newline));
}

/*
 * printf into a logfile with timestamp prefix
 */
void
log_tsprintf(const log_t *log, const char *format,...)
{
	if (!log_isopen(log))
		return;

	va_list args;
	char *message;
	char s_time[30];
	time_t now;
	struct tm *ltime;
	size_t tsize;
	char *newline = "\n";

	va_start(args, format);
	vasprintf(&message, format, args);
	va_end(args);

	now = time(NULL);
	ltime = localtime(&now);
	tsize = strftime(s_time, sizeof(s_time), "%Y-%m-%d %T %Z - ", ltime);

	write(log->fd, s_time, tsize);
	write(log->fd, message, strnlen(message, MAX_MSG_SIZE));
	write(log->fd, newline, sizeof(*newline));
}

/*
 * check integrity of logfile with comparing filesystem stat with our use-specified settings
 */
bool
log_verify(const log_t *log)
{
	struct stat sb;

	if (log == NULL || log->fd == -1) {
		errno = EINVAL;
		return (false);
	}
	if (fstat(log->fd, &sb) == -1)
		return (false);

	if (log->ino != sb.st_ino || log->dev != sb.st_dev || log->mode != sb.st_mode) {
		errno = EINVAL;
		return (false);
	}
	return true;
}
