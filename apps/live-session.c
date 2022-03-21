/**
 * @brief live-session - Run live CD user session.
 *
 * Launches the general session manager as 'local', waits for the
 * session to end, then launches the login manager.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2018 K. Lange
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <toaru/auth.h>
#include <toaru/yutani.h>
#include <toaru/trace.h>
#define TRACE_APP_NAME "live-session"

int main(int argc, char * argv[]) {
	int pid;

	if (geteuid() != 0) {
		return 1;
	}

	int _session_pid = fork();
	if (!_session_pid) {
		toaru_set_credentials(1000);
		char * args[] = {"/bin/session", NULL};
		execvp(args[0], args);

		return 1;
	}

	/* Dummy session for live-session prevents compositor from killing itself
	 * when the main session dies the first time. */
	yutani_init();

	do {
		pid = wait(NULL);
	} while ((pid > 0 && pid != _session_pid) || (pid == -1 && errno == EINTR));

	TRACE("Live session has ended, launching graphical login.");
	int _glogin_pid = fork();
	if (!_glogin_pid) {
		char * args[] = {"/bin/glogin",NULL};
		execvp(args[0],args);
		system("reboot");
	}

	do {
		pid = wait(NULL);
	} while ((pid > 0 && pid != _glogin_pid) || (pid == -1 && errno == EINTR));

	return 0;
}
