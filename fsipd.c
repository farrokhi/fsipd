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
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <ifaddrs.h>

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
#include <libutil.h>

/* Globals */
struct pidfh *pfh;

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
	case SIGINT:
	case SIGTERM:
		daemon_shutdown();
		exit(EXIT_SUCCESS);
		break;
	default:
		break;
	}
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

	char *no_fork = getenv("no_fork");

	if (!no_fork || strcmp("1", no_fork)) {

		/* Check if parent process id is set */
		if (getppid() == 1) {
			/* PPID exists, therefore we are already a daemon */
			return (EXIT_FAILURE);
		}
		/* Check if we can acquire the pid file */
		pfh = pidfile_open(NULL, 0600, &otherpid);

		if (pfh == NULL) {
			if (errno == EEXIST) {
				errx(EXIT_FAILURE, "Daemon already running, pid: %jd.", (intmax_t)otherpid);
			}
			warn("Cannot open or create pidfile.");
		}
		/* fork ourselves if not asked otherwise */
		if (fork()) {
			return (EXIT_SUCCESS);
		}
		/* we are the child, complete the daemonization */

		/* Close standard IO */
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);

		/* Block unnecessary signals */
		sigemptyset(&sig_set);
		sigaddset(&sig_set, SIGCHLD);	/* ignore child - i.e. we
						 * don't need to wait for it */
		sigaddset(&sig_set, SIGTSTP);	/* ignore Tty stop signals */
		sigaddset(&sig_set, SIGTTOU);	/* ignore Tty background
						 * writes */
		sigaddset(&sig_set, SIGTTIN);	/* ignore Tty background reads */
		sigprocmask(SIG_BLOCK, &sig_set, NULL);	/* Block the above
							 * specified signals */

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

	}

	/* TODO: Network Logic Here */
	while (1) {

		sleep(5);

	}

	return (0);
}

int 
main(int argc, char *argv[])
{
	if (argc > 1) {
		char *first_arg = argv[1];

		if (!strcmp(first_arg, "start")) {
			return daemon_start();
		}
	}
	return (EXIT_SUCCESS);
}
