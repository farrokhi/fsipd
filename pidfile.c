#include "pidfile.h"


struct pidfh {
	int		pf_fd;
	char	pf_path[MAXPATHLEN + 1];
	dev_t	pf_dev;
	ino_t	pf_ino;
};

static int _pidfile_remove(struct pidfh *pfh, int freeit);

static int
pidfile_verify(const struct pidfh *pfh)
{       
        struct stat sb;
        
        if (pfh == NULL || pfh->pf_fd == -1)
                return (EINVAL);
        /*
         * Check remembered descriptor.
         */
        if (fstat(pfh->pf_fd, &sb) == -1)
                return (errno);
        if (sb.st_dev != pfh->pf_dev || sb.st_ino != pfh->pf_ino)
                return (EINVAL);
        return (0);
}

int
flopen(const char *path, int flags, ...)
{
        int fd, operation, serrno, trunc;
        struct stat sb, fsb;
        int mode;

#ifdef O_EXLOCK
        flags &= ~O_EXLOCK;
#endif

        mode = 0;
        if (flags & O_CREAT) {
                va_list ap;

                va_start(ap, flags);
                mode = (int)va_arg(ap, int);
                va_end(ap);
        }

        operation = LOCK_EX;
        if (flags & O_NONBLOCK)
                operation |= LOCK_NB;

        trunc = (flags & O_TRUNC);
        flags &= ~O_TRUNC;

        for (;;) {
                if ((fd = open(path, flags, mode)) == -1)
                        /* non-existent or no access */
                        return (-1);
                if (flock(fd, operation) == -1) {
                        /* unsupported or interrupted */
                        serrno = errno;
                        (void)close(fd);
                        errno = serrno;
                        return (-1);
                }
                if (stat(path, &sb) == -1) {
                        /* disappeared from under our feet */
                        (void)close(fd);
                        continue;
                }
                if (fstat(fd, &fsb) == -1) {
                        /* can't happen [tm] */
                        serrno = errno;
                        (void)close(fd);
                        errno = serrno;
                        return (-1);
                }
                if (sb.st_dev != fsb.st_dev ||
                    sb.st_ino != fsb.st_ino) {
                        /* changed under our feet */
                        (void)close(fd);
                        continue;
                }
                if (trunc && ftruncate(fd, 0) != 0) {
                        /* can't happen [tm] */
                        serrno = errno;
                        (void)close(fd);
                        errno = serrno;
                        return (-1);
                }
                return (fd);
        }
}

static int
pidfile_read(const char *path, pid_t *pidptr)
{
        char buf[16], *endptr;
        int error, fd, i;

        fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd == -1)
                return (errno);

        i = read(fd, buf, sizeof(buf) - 1);
        error = errno;  /* Remember errno in case close() wants to change it. */
        close(fd);
        if (i == -1)
                return (error);
        else if (i == 0)
                return (EAGAIN);
        buf[i] = '\0';

        *pidptr = strtol(buf, &endptr, 10);
        if (endptr != &buf[i])
                return (EINVAL);

        return (0);
}

struct pidfh *pidfile_open(const char *path, mode_t mode, pid_t *pidptr)
{
	struct pidfh *pfh;
	struct timespec rqtp;
	struct stat sb;
	int len, fd, error, count;

	pfh = malloc(sizeof(*pfh));
	if (pfh == NULL) return NULL;

	if (path == NULL) 
		len = snprintf(pfh->pf_path, sizeof(pfh->pf_path), "/var/run/%s.pid", getprogname());
	else
		len = snprintf(pfh->pf_path, sizeof(pfh->pf_path), "%s", path);

	if (len >= sizeof(pfh->pf_path)) {
		free(pfh);
		errno = ENAMETOOLONG;
		return(NULL);
	}

	fd = flopen(pfh->pf_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NONBLOCK, mode);

	if (fd == -1) {
		if (errno == EWOULDBLOCK) {
		if (pidptr == NULL) {
			errno = EEXIST;
		} else {
			count = 20;
			rqtp.tv_sec = 0;
			rqtp.tv_nsec = 5000000;
			for (;;) {
				errno = pidfile_read(pfh->pf_path,pidptr);
				if (errno != EAGAIN || --count == 0) break;
				nanosleep(&rqtp, 0);
			}
			if (errno == EAGAIN) *pidptr = -1;
			if (errno == 0 || errno == EAGAIN) errno = EEXIST;
		}
	}
	free(pfh);
	return(NULL);
	}
	if (fstat(fd, &sb) == -1) {
		error = errno;
		unlink(pfh->pf_path);
		close(fd);
		free(pfh);
		errno = error;
		return(NULL);
	}
	pfh->pf_fd = fd;
	pfh->pf_dev = sb.st_dev;
	pfh->pf_ino = sb.st_ino;

	return(pfh);
}

static int
_pidfile_remove(struct pidfh *pfh, int freeit)
{
        int error;

        error = pidfile_verify(pfh);
        if (error != 0) {
                errno = error;
                return (-1);
        }

        if (unlink(pfh->pf_path) == -1)
                error = errno;
        if (close(pfh->pf_fd) == -1) {
                if (error == 0)
                        error = errno;
        }
        if (freeit)
                free(pfh);
        else
                pfh->pf_fd = -1;
        if (error != 0) {
                errno = error;
                return (-1);
        }
        return (0);
}

int
pidfile_remove(struct pidfh *pfh)
{

        return (_pidfile_remove(pfh, 1));
}

int
pidfile_write(struct pidfh *pfh)
{
        char pidstr[16];
        int error, fd;

        /*
         * Check remembered descriptor, so we don't overwrite some other
         * file if pidfile was closed and descriptor reused.
         */
        errno = pidfile_verify(pfh);
        if (errno != 0) {
                /*
                 * Don't close descriptor, because we are not sure if it's ours.
                 */
                return (-1);
        }
        fd = pfh->pf_fd;

        /*
         * Truncate PID file, so multiple calls of pidfile_write() are allowed.
         */
        if (ftruncate(fd, 0) == -1) {
                error = errno;
                _pidfile_remove(pfh, 0);
                errno = error;
                return (-1);
        }

        snprintf(pidstr, sizeof(pidstr), "%u", getpid());
        if (pwrite(fd, pidstr, strlen(pidstr), 0) != (ssize_t)strlen(pidstr)) {
                error = errno;
                _pidfile_remove(pfh, 0);
                errno = error;
                return (-1);
        }

        return (0);
}

int
pidfile_close(struct pidfh *pfh)
{
        int error;

        error = pidfile_verify(pfh);
        if (error != 0) {
                errno = error;
                return (-1);
        }

        if (close(pfh->pf_fd) == -1)
                error = errno;
        free(pfh);
        if (error != 0) {
                errno = error;
                return (-1);
        }
        return (0);
}

int
pidfile_fileno(const struct pidfh *pfh)
{

        if (pfh == NULL || pfh->pf_fd == -1) {
                errno = EINVAL;
                return (-1);
        }
        return (pfh->pf_fd);
}
