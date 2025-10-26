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

#define _POSIX_C_SOURCE 200809L

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <pidutil.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "banned.h"
#include "logfile.h"
#include "syslog_names.h"

#define PORT 5060
#define BACKLOG 1024

#ifndef IPV6_BINDV6ONLY /* Linux does not have IPV6_BINDV6ONLY */
#define IPV6_BINDV6ONLY IPV6_V6ONLY
#endif /* IPV6_BINDV6ONLY */

/*
 * Globals
 */
log_t	     *lfh;
struct pidfh *pfh;
bool	      use_syslog  = false;
char	     *logfilename = NULL;
char	     *pidfilename = NULL;
int	      syslog_pri  = -1;

/* Self-pipe for async-signal-safe signal handling */
static int		     signal_pipe[2] = { -1, -1 };
static volatile sig_atomic_t shutdown_flag	 = 0;

/* Mutex to protect log access from concurrent threads */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr_in t_sa, u_sa;
int		   t_sockfd, u_sockfd;

#ifdef PF_INET6
struct sockaddr_in6 t6_sa, u6_sa;
int		    t6_sockfd, u6_sockfd;

#endif /* PF_INET6 */

/*
 * trim string from whitespace characters
 */
size_t
chomp(char *s)
{
	int i;

	/* trim leading spaces */
	while (isspace(*s))
		s++;

	/* All spaces? */
	if (*s == 0)
		return 0;

	/* trim trailing spaces */
	i = strlen(s);

	while ((i > 0) && (isspace(s[i - 1])))
		i--;

	s[i] = '\0';

	return i;
}

/*
 * Prepare for a clean shutdown
 * Safe to call multiple times (idempotent)
 */
void
daemon_shutdown()
{
	static sig_atomic_t already_shutdown = 0;

	/* Prevent repeated execution */
	if (already_shutdown)
		return;
	already_shutdown = 1;

	pidfile_remove(pfh);
	if (!use_syslog) {
		log_close(lfh);
		lfh = NULL;
	}

	/* Free allocated strings and set to NULL to prevent double-free */
	if (logfilename != NULL) {
		free(logfilename);
		logfilename = NULL;
	}
	if (pidfilename != NULL) {
		free(pidfilename);
		pidfilename = NULL;
	}
}

/*
 * Async-signal-safe signal handler
 * Only writes signal number to pipe for main loop to process
 * Pipe is non-blocking so write won't deadlock if pipe is full
 */
void
signal_handler(int sig)
{
	unsigned char sig_byte = (unsigned char)sig;
	ssize_t	      ret;

	/* Write is async-signal-safe, pipe is non-blocking (EAGAIN is fine) */
	ret = write(signal_pipe[1], &sig_byte, 1);
	(void)ret; /* Ignore all errors including EAGAIN */
}

/*
 * Process signals received via self-pipe (called from main loop)
 * Safe to call non-async-signal-safe functions here
 */
static bool
process_signal(unsigned char sig)
{
	switch (sig) {
	case SIGHUP:
		if (!use_syslog) {
			/* Acquire mutex to prevent race with worker threads */
			pthread_mutex_lock(&log_mutex);
			log_reopen(&lfh);
			pthread_mutex_unlock(&log_mutex);
		}
		break;
	case SIGINT:
	case SIGTERM:
		/* Set flag first to stop new logging attempts */
		shutdown_flag = 1;
		/* Acquire mutex to ensure no thread is in log_printf */
		pthread_mutex_lock(&log_mutex);
		daemon_shutdown();
		pthread_mutex_unlock(&log_mutex);
		return true; /* Signal shutdown */
	default:
		break;
	}
	return false; /* Continue running */
}

void
process_request(int af, struct sockaddr *src, int proto, char *str)
{
	char		   *p_names[] = { "TCP", "UDP", "RAW", "UNKNOWN" };
	char		   *pname;
	uint16_t	    port;
	char		    addr_str[46];
	struct sockaddr_in *s_in;

#ifdef PF_INET6
	struct sockaddr_in6 *s_in6;

#endif /* PF_INET6 */

	switch (proto) {
	case SOCK_STREAM:
		pname = p_names[0];
		break;
	case SOCK_DGRAM:
		pname = p_names[1];
		break;
	case SOCK_RAW:
		pname = p_names[2];
		break;
	default:
		pname = p_names[3];
		;
	}

	chomp(str);

#ifdef PF_INET6
	switch (af) {
	case AF_INET6:
		s_in6 = (struct sockaddr_in6 *)src;
		inet_ntop(af, &s_in6->sin6_addr, addr_str, sizeof(addr_str));
		port = ntohs(s_in6->sin6_port);
		if (use_syslog) {
			syslog(syslog_pri, "From: %s:%d (%s6) - Message: \"%s\"", addr_str, port,
			    pname, str);
		} else {
			pthread_mutex_lock(&log_mutex);
			/* Re-check shutdown_flag after acquiring mutex to prevent TOCTOU */
			if (!shutdown_flag && lfh != NULL) {
				log_printf(lfh, "%ld,%s6,%s,%d,\"%s\"", time(NULL), pname,
				    addr_str, port, str);
			}
			pthread_mutex_unlock(&log_mutex);
		}
		break;
	case AF_INET:
		s_in = (struct sockaddr_in *)src;
		inet_ntop(af, &s_in->sin_addr, addr_str, sizeof(addr_str));
		port = ntohs(s_in->sin_port);
		if (use_syslog) {
			syslog(syslog_pri, "From: %s:%d (%s4) - Message: \"%s\"", addr_str, port,
			    pname, str);
		} else {
			pthread_mutex_lock(&log_mutex);
			/* Re-check shutdown_flag after acquiring mutex to prevent TOCTOU */
			if (!shutdown_flag && lfh != NULL) {
				log_printf(lfh, "%ld,%s4,%s,%d,\"%s\"", time(NULL), pname,
				    addr_str, port, str);
			}
			pthread_mutex_unlock(&log_mutex);
		}
		break;
	}
#else
	s_in = (struct sockaddr_in *)src;
	inet_ntop(af, &s_in->sin_addr, addr_str, sizeof(addr_str));
	port = ntohs(s_in->sin_port);
	if (use_syslog) {
		syslog(syslog_pri, "From: %s:%d (%s4) - Message: \"%s\"", addr_str, port, pname,
		    str);
	} else {
		pthread_mutex_lock(&log_mutex);
		/* Re-check shutdown_flag after acquiring mutex to prevent TOCTOU */
		if (!shutdown_flag && lfh != NULL) {
			log_printf(lfh, "%ld,%s4,%s,%d,\"%s\"", time(NULL), pname, addr_str, port,
			    str);
		}
		pthread_mutex_unlock(&log_mutex);
	}
#endif
}

/*
 * setup TCP listener socket
 */
int
init_tcp()
{

#ifdef PF_INET6
	/* Setup TCP6 Listener */
	memset(&t6_sa, 0, sizeof(t6_sa));
	t6_sa.sin6_port	    = htons(PORT);
	t6_sa.sin6_family   = AF_INET6;
	t6_sa.sin6_addr	    = in6addr_any;
	t6_sa.sin6_scope_id = 0;
	if ((t6_sockfd = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
		perror("tcp6 socket()");
		return (EXIT_FAILURE);
	}
	int on = 1;

	setsockopt(t6_sockfd, IPPROTO_IPV6, IPV6_BINDV6ONLY, (char *)&on, sizeof(on));

	if (bind(t6_sockfd, (struct sockaddr *)&t6_sa, sizeof(t6_sa)) < 0) {
		perror("tcp6 bind()");
		return (EXIT_FAILURE);
	}
#endif /* PF6_INET */

	/* Setup TCP4 Listener */
	memset(&t_sa, 0, sizeof(t_sa));
	t_sa.sin_port	     = htons(PORT);
	t_sa.sin_family	     = AF_INET;
	t_sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((t_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcp4 socket()");
		return (EXIT_FAILURE);
	}
	if (bind(t_sockfd, (struct sockaddr *)&t_sa, sizeof(t_sa)) < 0) {
		perror("tcp4 bind()");
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/*
 * setup UDP listener socket
 */
int
init_udp()
{

#ifdef PF_INET6

	/* Setup UDP6 Listener */
	memset(&u6_sa, 0, sizeof(u6_sa));
	u6_sa.sin6_port	    = htons(PORT);
	u6_sa.sin6_family   = AF_INET6;
	u6_sa.sin6_addr	    = in6addr_any;
	u6_sa.sin6_scope_id = 0;
	if ((u6_sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("udp6 socket()");
		return (EXIT_FAILURE);
	}
	int on = 1;

	setsockopt(u6_sockfd, IPPROTO_IPV6, IPV6_BINDV6ONLY, (char *)&on, sizeof(on));

	if (bind(u6_sockfd, (struct sockaddr *)&u6_sa, sizeof(u6_sa)) < 0) {
		perror("udp6 bind()");
		return (EXIT_FAILURE);
	}
#endif /* PF_INET6 */

	/* Setup UDP4 Listener */
	memset(&u_sa, 0, sizeof(u_sa));
	u_sa.sin_port	     = htons(PORT);
	u_sa.sin_family	     = AF_INET;
	u_sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((u_sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("udp4 socket()");
		return (EXIT_FAILURE);
	}
	if (bind(u_sockfd, (struct sockaddr *)&u_sa, sizeof(u_sa)) < 0) {
		perror("udp4 bind()");
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

void *
tcp4_handler(void *args)
{
	int		   c;
	struct sockaddr_in t_other;
	FILE		  *client;
	char		   str[8192];
	socklen_t	   sa_len;

	listen(t_sockfd, BACKLOG);

	while (1) {
		sa_len = sizeof(t_sa);
		if ((c = accept(t_sockfd, (struct sockaddr *)&t_other, &sa_len)) < 0) {
			perror("tcp accept()");
			continue;
		}
		if ((client = fdopen(c, "r")) == NULL) {
			perror("tcp fdopen()");
			close(c);
			continue;
		}
		memset(str, 0, sizeof(str)); /* just in case */
		if (fgets(str, sizeof(str), client) != NULL) {
			process_request(t_other.sin_family, (struct sockaddr *)&t_other,
			    SOCK_STREAM, str);
		}
		/* Shutdown write end to signal FIN, avoiding CLOSE_WAIT */
		shutdown(c, SHUT_WR);
		fclose(client);
	}
	return (args); /* suppress compiler warning */
}

void *
udp4_handler(void *args)
{
	char		   str[8192];
	struct sockaddr_in u_other;
	socklen_t	   sa_len;
	ssize_t		   len;

	sa_len = sizeof(u_other);
	while (1) {
		len = recvfrom(u_sockfd, str, sizeof(str) - 1, 0, (struct sockaddr *)&u_other,
		    &sa_len);
		if (len > 0) {
			str[len] = '\0';
			process_request(u_other.sin_family, (struct sockaddr *)&u_other, SOCK_DGRAM,
			    str);
		} else if (len < 0) {
			perror("udp4 recvfrom()");
		}
	}

	return (args); /* suppress compiler warning */
}

#ifdef PF_INET6

void *
tcp6_handler(void *args)
{
	int		    c;
	struct sockaddr_in6 t_other;
	FILE		   *client;
	char		    str[8192];
	socklen_t	    sa_len;

	listen(t6_sockfd, BACKLOG);

	while (1) {
		sa_len = sizeof(t6_sa);
		if ((c = accept(t6_sockfd, (struct sockaddr *)&t_other, &sa_len)) < 0) {
			perror("tcp6 accept()");
			continue;
		}
		if ((client = fdopen(c, "r")) == NULL) {
			perror("tcp6 fdopen()");
			close(c);
			continue;
		}
		memset(str, 0, sizeof(str)); /* just in case */
		if (fgets(str, sizeof(str), client) != NULL) {
			process_request(t_other.sin6_family, (struct sockaddr *)&t_other,
			    SOCK_STREAM, str);
		}
		/* Shutdown write end to signal FIN, avoiding CLOSE_WAIT */
		shutdown(c, SHUT_WR);
		fclose(client);
	}
	return (args); /* suppress compiler warning */
}

void *
udp6_handler(void *args)
{
	char		    str[8192];
	struct sockaddr_in6 u_other;
	socklen_t	    sa_len;
	ssize_t		    len;

	sa_len = sizeof(u_other);
	while (1) {
		len = recvfrom(u6_sockfd, str, sizeof(str) - 1, 0, (struct sockaddr *)&u_other,
		    &sa_len);
		if (len > 0) {
			str[len] = '\0';
			process_request(u_other.sin6_family, (struct sockaddr *)&u_other,
			    SOCK_DGRAM, str);
		} else if (len < 0) {
			perror("udp6 recvfrom()");
		}
	}

	return (args); /* suppress compiler warning */
}

#endif /* PF_INET6 */

void
init_logger()
{
	if (use_syslog) {
		/* initialize facility and level parameters */
		if (syslog_pri == -1) /* not specidied by user, use default */
			syslog_pri = LOG_USER | LOG_NOTICE | LOG_PID;
	} else {
		/* open a log file in current directory */
		if (logfilename == NULL) {
			logfilename = strdup("fsipd.log");
			if (logfilename == NULL)
				err(EXIT_FAILURE, "Cannot allocate memory for log filename");
		}
		if ((lfh = log_open(logfilename, 0644)) == NULL)
			err(EXIT_FAILURE, "Cannot open log file \"%s\"", logfilename);
	}
}

/*
 * Daemonize and persist pid
 */
int
daemon_start()
{
	struct sigaction sig_action;
	sigset_t	 sig_set;
	pid_t		 otherpid;
	int		 curPID;
	pthread_t	 tcp4_thread, udp4_thread;
	pthread_t	 tcp6_thread, udp6_thread;

	/* Create self-pipe for signal handling */
	if (pipe(signal_pipe) == -1) {
		err(EXIT_FAILURE, "Cannot create signal pipe");
	}

	/* Set both ends non-blocking to prevent signal handler deadlock */
	if (fcntl(signal_pipe[0], F_SETFL, O_NONBLOCK) == -1 ||
	    fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK) == -1) {
		err(EXIT_FAILURE, "Cannot set signal pipe non-blocking");
	}

	/* Check if we can acquire the pid file */
	pfh = pidfile_open(pidfilename, 0644, &otherpid);

	if (pfh == NULL) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.", (intmax_t)otherpid);
		}
		err(EXIT_FAILURE, "Cannot open or create pidfile");
	}
	init_logger();

	/* Initialize TCP46 and UDP46 sockets */
	if (init_tcp() == EXIT_FAILURE)
		return (EXIT_FAILURE);
	if (init_udp() == EXIT_FAILURE)
		return (EXIT_FAILURE);

	/* start daemonizing */
	curPID = fork();

	switch (curPID) {
	case 0: /* This process is the child */
		break;
	case -1: /* fork() failed, should exit */
		perror("fork");
		return (EXIT_FAILURE);
	default: /* fork() successful, should exit
		  * (parent) */
		return (EXIT_SUCCESS);
	}

	/* we are the child, complete the daemonization */

	/* Close standard IO */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	/* Block unnecessary signals */
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGCHLD);		/* ignore child - i.e. we don't need
						 * to wait for it */
	sigaddset(&sig_set, SIGTSTP);		/* ignore tty stop signals */
	sigaddset(&sig_set, SIGTTOU);		/* ignore tty background writes */
	sigaddset(&sig_set, SIGTTIN);		/* ignore tty background reads */
	sigprocmask(SIG_BLOCK, &sig_set, NULL); /* Block the above specified
						 * signals */

	/* Catch necessary signals */
	sig_action.sa_handler = signal_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;

	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGHUP, &sig_action, NULL);
	sigaction(SIGINT, &sig_action, NULL);

	/* create new session and process group */
	setsid();

	/* persist pid */
	pidfile_write(pfh);

	/* Create TCP and UDP listener threads */
	pthread_create(&tcp4_thread, NULL, tcp4_handler, NULL);
	pthread_create(&udp4_thread, NULL, udp4_handler, NULL);
#ifdef PF_INET6
	pthread_create(&tcp6_thread, NULL, tcp6_handler, NULL);
	pthread_create(&udp6_thread, NULL, udp6_handler, NULL);
#endif

	/*
	 * Main event loop: monitor signal pipe for shutdown/reload requests
	 * Threads run independently and normally never terminate
	 */
	while (1) {
		fd_set	      read_fds;
		unsigned char sig_byte;
		ssize_t	      n;

		FD_ZERO(&read_fds);
		FD_SET(signal_pipe[0], &read_fds);

		/* Wait for signal notification (no timeout) */
		if (select(signal_pipe[0] + 1, &read_fds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue; /* Interrupted by signal, retry */
			break;	  /* Other error, exit */
		}

		/* Read signal from pipe */
		n = read(signal_pipe[0], &sig_byte, 1);
		if (n <= 0)
			continue;

		/* Process signal and check for shutdown */
		if (process_signal(sig_byte)) {
			/* Shutdown requested */
			break;
		}
	}

	/* Cleanup */
	close(signal_pipe[0]);
	close(signal_pipe[1]);

	return (EXIT_SUCCESS);
}

void
usage()
{
	printf("usage: fsipd [-h] [-l logfile] [-s] [-p priority] [-f pidfile]\n");
	printf("\t-h: this message\n");
	printf("\t-s: use syslog instead of local log file\n");
	printf("\t-p: syslog priotiry (default: user.notice)\n");
	printf("\t-l: specify output log filename (default: fsipd.log)\n");
	printf("\t-f: specify pid file path (default: /var/run/fsipd.pid)\n");
}

static int
decode(char *name, const CODE *codetab)
{
	const CODE *c;

	if (isdigit(*name))
		return (atoi(name));

	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return (-1);
}

static int
decodepri(char *s)
{
	char *save;
	int   fac, lev;

	for (save = s; *s && *s != '.'; ++s)
		;
	if (*s) {
		*s  = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0)
			errx(1, "unknown facility name: %s", save);
		*s++ = '.';
	} else {
		fac = 0;
		s   = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0)
		errx(1, "unknown priority name: %s", save);
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "hl:sp:f:")) != -1) {
		switch (opt) {
		case 's':
			use_syslog = true;
			break;
		case 'p':
			syslog_pri = decodepri(optarg) | LOG_PID;
			break;
		case 'l':
			logfilename = strdup(optarg);
			if (logfilename == NULL)
				err(EXIT_FAILURE, "Cannot allocate memory");
			break;
		case 'f':
			pidfilename = strdup(optarg);
			if (pidfilename == NULL)
				err(EXIT_FAILURE, "Cannot allocate memory");
			break;
		case 'h':
			usage();
			exit(0);
			break;
		default:
			printf("invalid option: %c\n", opt);
			usage();
			exit(1);
		}
	}

	return (daemon_start());
}
