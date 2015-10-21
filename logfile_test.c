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

#include <stdio.h>
#include <err.h>
#include <errno.h>
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

	log_printf(lh, "opened file handle: %d , inode: %llu\n", lh->fd, lh->ino);
	printf("logfile: %s, handle: %d, inode: %llu, mode: %d\n", lh->path, lh->fd, lh->ino, lh->mode);

	log_reopen(&lh);
	if (!log_verify(lh))
		err(errno, "Failed to verify integrity of reopened log file");

	log_printf(lh, "reopened file handle: %d , inode: %llu\n", lh->fd, lh->ino);
	printf("logfile: %s, handle: %d, inode: %llu, mode: %d\n", lh->path, lh->fd, lh->ino, lh->mode);

	log_close(lh);

	return 0;
}
