/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2025 Babak Farrokhi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * LICENSE file are met.
 */

#ifndef _LOGFILE_H
#define _LOGFILE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOGPATH "/var/log"
#define MAX_MSG_SIZE 65536

typedef struct _log_t {
	int    fd;
	char   path[MAXPATHLEN + 1];
	dev_t  dev;
	ino_t  ino;
	mode_t mode;
} log_t;

log_t *log_open(const char *path, mode_t mode);
void   log_close(const log_t *log);
bool   log_isopen(const log_t *log);
bool   log_verify(const log_t *log);
void   log_reopen(log_t **log);
void   log_printf(const log_t *log, const char *format, ...);
void   log_tsprintf(const log_t *log, const char *format, ...);

#endif /* _LOGFILE_H */
