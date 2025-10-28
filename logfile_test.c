/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2025 Babak Farrokhi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * LICENSE file are met.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <sysexits.h>

#include "logfile.h"

/*
 * dummy program to test logfile functionality
 */

int
main(void)
{
	log_t *lh;

	if ((lh = log_open("test.log", 0600)) == NULL) {
		err(EX_IOERR, "Cannot open log file");
	}
	if (!log_verify(lh))
		err(errno, "Failed to verify integrity of log file");

	log_printf(lh, "opened file handle: %d , inode: %llu", lh->fd, (unsigned long long)lh->ino);
	printf("logfile: %s, handle: %d, inode: %llu, mode: %d\n", lh->path, lh->fd,
	    (unsigned long long)lh->ino, lh->mode);

	log_reopen(&lh);
	if (!log_verify(lh))
		err(errno, "Failed to verify integrity of reopened log file");

	log_printf(lh, "reopened file handle: %d , inode: %llu", lh->fd,
	    (unsigned long long)lh->ino);
	printf("logfile: %s, handle: %d, inode: %llu, mode: %d\n", lh->path, lh->fd,
	    (unsigned long long)lh->ino, lh->mode);

	for (int i = 1; i <= 4; i++)
		log_tsprintf(lh, "This is a time stamped message %d", i);

	log_close(lh);

	return 0;
}
