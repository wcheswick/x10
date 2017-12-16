#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "x10.h"

static void
send_command(char *cmd) {
	int n;
	int fd;

	alarm(2);

	if (debug > 1)
		fprintf(stderr, "send_command: opening %s\n", X10CTL);
	if ((fd = open(X10CTL, O_WRONLY)) < 0) {
		perror("send_command: opening controller pipe");
		exit(1);
	}

	if (debug > 0)
		fprintf(stderr, "send_command: writing command  %s\n", cmd);
	n = write(fd, cmd, strlen(cmd)+1);
	if (n < 0) {
		perror("send_command: writing command\n");
		exit(2);
	}
	if (debug > 1)
		fprintf(stderr, "send_command: resetting alarm\n");
	alarm(0);
	close(fd);
}

static void
send_commands(char *cmds) {
	char *cp;

	do {
		cp = strchr(cmds, ';');
		if (cp) {
			*cp++ = '\0';
			if (*cp == ' ')
				cp++;
		}
		send_command(cmds);
		cmds = cp;
	} while (cmds && *cmds);
}

char *client_cmd = 0;

static void
client_timeout(int i) {
	fprintf(stderr, "x10 daemon unresponsive. aborting  '%s'\n",
		client_cmd);
	exit(1);
}

int
do_client(int argc, char *argv[]) {
	if (asprintf(&client_cmd, "%s", *argv++) < 0) {
		perror("do_client, asprintf 1");
		return 1;
	}

	while (*argv) {
		char *lbp = client_cmd;
		if (asprintf(&client_cmd, "%s %s", lbp, *argv++) < 0) {
			perror("do_client, asprintf 2");
			return 2;
		}
		free(lbp);
	}

	signal(SIGALRM, client_timeout);
	send_commands(client_cmd);
	free(client_cmd);

	return 0;
}
