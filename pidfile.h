#ifndef _PIDFILE_H
#define _PIDFILE_H

#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

struct pidfh;

int	flopen(const char *_path, int _flags, ...);

struct pidfh *pidfile_open(const char *path, mode_t mode, pid_t *pidptr);
int pidfile_write(struct pidfh	*pfh);
int pidfile_close(struct pidfh	*pfh);
int pidfile_remove(struct pidfh *pfh);
int pidfile_fileno(const struct pidfh *pfh);


#endif /* _PIDFILE_H */