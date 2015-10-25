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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <pidutil.h>

#include "logfile.h"

#define	PORT 5060
#define BACKLOG 1024

/*
 * Globals
 */
struct pidfh *pfh;
struct protoent *proto_tcp, *proto_udp;
struct sockaddr_in t_sa, u_sa;
int	t_sockfd, u_sockfd;
log_t  *lfh;

/*
 * Interface
 */
void   *tcp_handler(void *args);
void   *udp_handler(void *args);

/*
 * remove training newline character from string
 */
void
chomp(char *s)
{
	char *p;

	while (NULL != s && NULL != (p = strrchr(s, '\n'))) {
		*p = '\0';
	}
}

/*
 * Prepare for a clean shutdown
 */
void
daemon_shutdown()
{
	pidfile_remove(pfh);
	log_close(lfh);
}

/*
 * Act upon receiving signals
 */
void
signal_handler(int sig)
{
	switch (sig) {

	case SIGHUP:
		log_reopen(&lfh);
		break;
	case SIGINT:
	case SIGTERM:
		daemon_shutdown();
		exit(EXIT_SUCCESS);
		break;
	default:
		break;
	}
}

void
process_request(const struct sockaddr_in *sa, int type, char *str)
{
	char *s_types[] = {"TCP", "UDP", "RAW", "Unknown"};
	char *ptype;

	switch (type) {
	case SOCK_STREAM:
		ptype = s_types[0];
		break;
	case SOCK_DGRAM:
		ptype = s_types[1];
		break;
	case SOCK_RAW:
		ptype = s_types[2];
		break;
	default:
		ptype = s_types[3];;
	}

	chomp(str);
	log_printf(lfh, "%ld,%s,%s,%d,\"%s\"",
	    time(NULL), ptype, inet_ntoa(sa->sin_addr), ntohs(sa->sin_port), str);
}

/*
 * Daemonize and persist pid
 */
int
daemon_start()
{
	struct sigaction sig_action;
	sigset_t sig_set;
	pid_t otherpid;
	int curPID;
	pthread_t tcp_thread, udp_thread;

	/* Check if we can acquire the pid file */
	pfh = pidfile_open(NULL, 0600, &otherpid);

	if (pfh == NULL) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.", (intmax_t)otherpid);
		}
		err(EXIT_FAILURE, "Cannot open or create pidfile");
	}
	/* open a log file in current directory */
	if ((lfh = log_open("fsipd.log", 0644)) == NULL) {
		err(EXIT_FAILURE, "Cannot open log file");
	}
	/* setup TCP socket */
	if ((proto_tcp = getprotobyname("tcp")) == NULL)
		return (EXIT_FAILURE);

	bzero(&t_sa, sizeof(t_sa));
	t_sa.sin_port = htons(PORT);
	t_sa.sin_family = AF_INET;
	t_sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((t_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcp socket()");
		return (EXIT_FAILURE);
	}
	if (bind(t_sockfd, (struct sockaddr *)&t_sa, sizeof t_sa) < 0) {
		perror("tcp bind()");
		return (EXIT_FAILURE);
	}
	/* setup UDP socket */
	if ((proto_udp = getprotobyname("udp")) == NULL)
		return (EXIT_FAILURE);

	bzero(&u_sa, sizeof(u_sa));
	u_sa.sin_port = htons(PORT);
	u_sa.sin_family = AF_INET;
	u_sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((u_sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("udp socket()");
		return (EXIT_FAILURE);
	}
	if (bind(u_sockfd, (struct sockaddr *)&u_sa, sizeof u_sa) < 0) {
		perror("udp bind()");
		return (EXIT_FAILURE);
	}
	/* start daemonizing */
	curPID = fork();

	switch (curPID) {
	case 0:			/* This process is the child */
		break;
	case -1:			/* fork() failed, should exit */
		perror("fork");
		return (EXIT_FAILURE);
	default:			/* fork() successful, should exit
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
	sigaddset(&sig_set, SIGCHLD);	/* ignore child - i.e. we don't need
					 * to wait for it */
	sigaddset(&sig_set, SIGTSTP);	/* ignore Tty stop signals */
	sigaddset(&sig_set, SIGTTOU);	/* ignore Tty background writes */
	sigaddset(&sig_set, SIGTTIN);	/* ignore Tty background reads */
	sigprocmask(SIG_BLOCK, &sig_set, NULL);	/* Block the above specified
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

#ifdef DEBUG
	syslog(LOG_ALERT, "%s started with PID %d\n",
	    getprogname(), getpid());
#endif					/* __DEBUG__ */

	/* persist pid */
	pidfile_write(pfh);

	/* Create TCP and UDP listener threads */
	pthread_create(&tcp_thread, NULL, tcp_handler, NULL);
	pthread_create(&udp_thread, NULL, udp_handler, NULL);

	/*
	 * Wait until first or second thread terminates, which
	 * normally shouldn't ever happen
	 */
	pthread_join(tcp_thread, NULL);
	pthread_join(udp_thread, NULL);

	return (EXIT_SUCCESS);
}

void   *
tcp_handler(void *args)
{
	int c;
	struct sockaddr_in t_other;
	FILE *client;
	char str[8192];
	socklen_t sa_len;

	listen(t_sockfd, BACKLOG);

	while (1) {
		sa_len = sizeof(t_sa);
		if ((c = accept(t_sockfd, (struct sockaddr *)&t_other, &sa_len)) < 0) {
			perror("accept");
			pthread_exit(NULL);
		}
		if ((client = fdopen(c, "r")) == NULL) {
			perror("fdopen");
			pthread_exit(NULL);
		}
		bzero(str, sizeof(str));/* just in case */
		fgets(str, sizeof(str), client);
		process_request(&t_other, SOCK_STREAM, str);
		fclose(client);
	}
	return (args);			/* mute the compiler warning */
}

void   *
udp_handler(void *args)
{
	char str[8192];
	struct sockaddr_in u_other;
	socklen_t sa_len;
	ssize_t len;

	sa_len = sizeof(u_other);
	while (1) {
		if ((len = recvfrom(u_sockfd, str, sizeof(str), 0, (struct sockaddr *)&u_other, &sa_len)) > 0) {
			process_request(&u_other, SOCK_DGRAM, str);
		}
	}

	return (args);			/* mute the compiler warning */
}

int
main(void)
{
	return (daemon_start());
}
