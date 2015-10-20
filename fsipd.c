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
#include "pidfile.h"

#define	PORT 5060
#define BACKLOG 1024

/* Globals */
struct pidfh *pfh;
struct sockaddr_in sa;
struct protoent *proto_tcp, *proto_udp;

/*
 * Prepare for a clean shutdown
 */
void
daemon_shutdown()
{
	pidfile_remove(pfh);
}

/*
 * Act upon receiving signals
 */
void
signal_handler(int sig)
{
	switch (sig) {

	case SIGHUP:
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
process_request(char *str)
{
	/* check input str for SIP requests */

	syslog(LOG_ALERT, "sip: %s, sport: %d, payload: \"%s\"\n",
		inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), str);
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
	register int s, c;
	unsigned int b;
	FILE *client;
	char inputstr[8192];

	/* Check if we can acquire the pid file */
	pfh = pidfile_open(NULL, 0600, &otherpid);

	if (pfh == NULL) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.", (intmax_t)otherpid);
		}
		err(EXIT_FAILURE,"Cannot open or create pidfile");
	}
	/* setup socket */
	if ((proto_tcp = getprotobyname("tcp")) == NULL)
		return -1;

	bzero(&sa, sizeof(sa));
	sa.sin_port = htons(PORT);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}
	if (bind(s, (struct sockaddr *)&sa, sizeof sa) < 0) {
		perror("bind");
		return EXIT_FAILURE;
	}
	/* start daemonizing */
	curPID = fork();

	switch (curPID) {
	case 0:			/* This process is the child */
		break;
	case -1:			/* fork() failed, should exit */
		perror("fork");
		return (EXIT_FAILURE);
	default:			/* fork() successful, should exit */
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

	/* persist pid */
	pidfile_write(pfh);

	/* TODO: Network Logic Here */

	listen(s, BACKLOG);

	while (1) {

		b = sizeof(sa);
		if ((c = accept(s, (struct sockaddr *)&sa, &b)) < 0) {
			perror("accept");
			return (EXIT_FAILURE);
		}
		if ((client = fdopen(c, "r")) == NULL) {
			perror("fdopen");
			return (EXIT_FAILURE);
		}
		fgets(inputstr, sizeof(inputstr), client);

		process_request(inputstr);

		fclose(client);
	}

	return (EXIT_SUCCESS);
}

int
main(void)
{
	return (daemon_start());
}
