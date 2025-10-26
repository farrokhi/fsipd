
/*-
 * Copyright (c) 2016, Babak Farrokhi
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
#endif /* __linux__ */

/*
 * create/open given logfile and initialize appropriate struct
 */
log_t *
log_open(const char *path, mode_t mode)
{
	log_t	   *lh;
	struct stat sb;
	char	   *filename;
	char	   *allocated_filename = NULL;
	int	    fd;

	if (path == NULL) {
		if (asprintf(&allocated_filename, "%s/%s.log", LOGPATH, _PROGNAME) == -1)
			return (NULL);
		filename = allocated_filename;
	} else {
		filename = (char *)path;
	}

	/*
	 * Open file with O_NOFOLLOW to prevent symlink attacks
	 * Use O_NONBLOCK to avoid blocking on FIFOs, then clear it
	 */
	if ((fd = open(filename, O_WRONLY | O_APPEND | O_CREAT | O_SYNC | O_NOFOLLOW | O_NONBLOCK,
		 mode)) == -1) {
		free(allocated_filename);
		return (NULL);
	}

	/* Use fstat on opened fd to prevent TOCTOU race */
	if (fstat(fd, &sb) == -1) {
		close(fd);
		free(allocated_filename);
		return (NULL);
	}

	/* Verify it's a regular file, not a device, FIFO, etc. */
	if (!S_ISREG(sb.st_mode)) {
		close(fd);
		free(allocated_filename);
		errno = EINVAL;
		return (NULL);
	}

	/* Clear O_NONBLOCK now that we've validated the file type */
	if (fcntl(fd, F_SETFL, O_WRONLY | O_APPEND | O_SYNC) == -1) {
		close(fd);
		free(allocated_filename);
		return (NULL);
	}

	if (flock(fd, LOCK_EX) == -1) {
		close(fd);
		free(allocated_filename);
		return (NULL);
	}
	/* initialize data structure */
	lh = calloc(1, sizeof(log_t));
	if (lh == NULL) {
		close(fd);
		free(allocated_filename);
		return (NULL);
	}

	lh->fd	 = fd;
	lh->dev	 = sb.st_dev;
	lh->ino	 = sb.st_ino;
	lh->mode = sb.st_mode;
	snprintf(lh->path, sizeof(lh->path), "%s", filename);

	/* Free temporary buffer after copying to struct */
	free(allocated_filename);

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
inline bool
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
	log_t *oldlog;
	log_t *newlog;

	if (!log_isopen(*log))
		return;

	oldlog = *log;

	/* Open new log first, keeping old one valid */
	newlog = log_open(oldlog->path, oldlog->mode);

	/* Atomically swap: old log becomes invalid only after new log is ready */
	*log = newlog;

	/* Now safe to close old log */
	if (oldlog != NULL)
		log_close(oldlog);
}

/*
 * printf given text into logfile
 */

void
log_printf(const log_t *log, const char *format, ...)
{
	if (!log_isopen(log))
		return;

	va_list args;
	char   *message;
	char   *newline = "\n";
	ssize_t ret;

	va_start(args, format);
	if (vasprintf(&message, format, args) == -1) {
		va_end(args);
		return;
	}
	va_end(args);

	ret = write(log->fd, message, strnlen(message, MAX_MSG_SIZE));
	(void)ret;
	ret = write(log->fd, newline, 1);
	(void)ret;

	free(message);
}

/*
 * printf into a logfile with timestamp prefix
 */
void
log_tsprintf(const log_t *log, const char *format, ...)
{
	if (!log_isopen(log))
		return;

	va_list	   args;
	char	  *message;
	char	   s_time[30];
	time_t	   now;
	struct tm *ltime;
	size_t	   tsize;
	char	  *newline = "\n";
	ssize_t	   ret;

	va_start(args, format);
	if (vasprintf(&message, format, args) == -1) {
		va_end(args);
		return;
	}
	va_end(args);

	now   = time(NULL);
	ltime = localtime(&now);
	tsize = strftime(s_time, sizeof(s_time), "%Y-%m-%d %T %Z - ", ltime);

	ret = write(log->fd, s_time, tsize);
	(void)ret;
	ret = write(log->fd, message, strnlen(message, MAX_MSG_SIZE));
	(void)ret;
	ret = write(log->fd, newline, 1);
	(void)ret;

	free(message);
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
