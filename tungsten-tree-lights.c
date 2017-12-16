/*
 * This program issues X10 commands to several strings of Christmas
 * lights.  Our strings used to be twined on two light posts in front
 * of our house.  Now they lie on a boxwood by the garage, where all the
 * power cords can reach them.
 *
 * We implement not some tacky flashing thing, but use the X10 dimming
 * function to slowly switch colors.  If you aren't watching closely,
 * you will miss the slow changes between various states.  Some people
 * driving by have noticed the different colors, and assumed that we
 * actually changed lights on the poles!
 *
 * Of course, the same X10 signals could go straight to a Christmas tree.
 * It takes a lot of lights, because you have to decorate the tree several
 * times, one for each color.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "arg.h"
#include "x10.h"

#define NSTRINGS	5

#define MINS_BETWEEN_CHANGES	1	// The anthony constant

struct codes {
	int	house, unit;
} codes[] = {
	{'d', 12},
	{'d', 13},
	{'d', 14},
	{'d', 15},
	{'d', 16},
};

int debug = 0;

void
send_command(char *buf) {
	int n;
	int fd = open(X10CTL, O_WRONLY);

	if (fd < 0) {
		perror("open x10ctl file");
		exit(1);
	}
	n = write(fd, buf, strlen(buf)+1);
	if (n < 0) {
		perror("write x10ctl file");
		exit(1);
	}
	close(fd);
}

void
send_command2(char *cmd, int l) {
	char buf[100];

	sprintf(buf, "%s %c%d", cmd, codes[l-1].house, codes[l-1].unit);
	send_command(buf);
	sleep(1);	/* every command takes at least a second */
}

void
send_command3(char *cmd, int l, int c) {
	char buf[100];
	int delay = 1 + (c/7 + 1);

	sprintf(buf, "%s %c%d %d", cmd, codes[l-1].house, codes[l-1].unit, c);
	send_command(buf);
	sleep(delay);
}

int
card(int i) {
	int n = 0;

	while (i != 0) {
		n = n + (i & 1);
		i = i >> 1;
	}
	return n;
}

/*
 * determine our next pattern.  We have esthetic constraints.
 * Four strings.  Never less than 1 or more than 2 strings on.
 */
int
selecttarget(int old) {
	int target;

	while (1) {
		target = random() % (1<<NSTRINGS);
		if (card(target) > 2)
			continue;
		/*
		 * It must change.
		 */
		if (target == old) /* we must change */
			continue;
		if (old && ((target & old) == 0))	/* must have one old color on */
			continue; 
		break;
	}
	return target;
}

int running = 1;

/* shut down */
void
times_up(int i) {
	if (debug)
		fprintf(stderr, "Times up!\n");
	running = 0;
}

void
adjust_lights(int old, int target, int amount) {
	int j;

	if (debug)
		fprintf(stderr, "%.2x-->%.2x ", old, target);

	for (j=0; j<NSTRINGS; j++) { /*advance each string*/
		int mask = 1 << j;
		if (((old ^ target) & mask) == 0)
			continue;
		if (target & mask) {
			send_command3("bright", j+1, amount);
		} else {
			send_command3("dim", j+1, amount);
		}
	}
}

void
interesting(void) {
	int target, old = 0;	/*start with all strings on */
	int i;

	srandom(time(0));

	/*
	 * Initialize each x10 controller.  We don't want all the lights
	 * to be on at once...the breaker will blow.  So we turn on each
	 * color, and dim it down, one color at a time.
	 *
	 * We have LED lights now, which don't have this problem, but
	 * it doesn't hurt to keep the power-up procedure.
	 */

	if (debug)
		fprintf(stderr, "Turn all strings off...\n");
	for (i=1; i<=NSTRINGS; i++)
		send_command2("off", i);

	if (debug)
		fprintf(stderr, "...now turn them on, one by one...");
	for (i=1; i<=NSTRINGS; i++) {
		if (debug)
			fprintf(stderr, " %d", i);
		send_command2("on", i);
		send_command3("dim", i, 31);
	}
	if (debug)
		fprintf(stderr, "\n");

	while (running) {
		target = selecttarget(old);
		for (i=0; running && i<16; i++)
			adjust_lights(old, target, 2);
		adjust_lights(old, target, 31);	/* make sure they made it */
		old = target;
		if (running) {
			if (debug)
				sleep(10);
			else
				sleep(MINS_BETWEEN_CHANGES*60);
		}
	}
	if (debug)
		fprintf(stderr, "shutdown, turn strings off\n");
	for (i=1; i<=NSTRINGS; i++) {
		send_command2("off", i);
	}
}

int
usage(void) {
	fprintf(stderr, "usage: lights [-d] [hours]\n");
	return 1;
}
	
int
main(int argc, char *argv[]) {

	ARGBEGIN {
	case 'd': debug++;	break;
	default:
		return(usage());
	} ARGEND;

	if (argc == 1) {
		int limit = atoi(argv[0]);
		if (limit == 0)
			return(usage());
		
		signal(SIGALRM, times_up);
		alarm(limit*3600);
	}

	signal(SIGUSR1, times_up);
	signal(SIGHUP, times_up);
	signal(SIGINT, times_up);
	
	interesting();
	return 0;
}
