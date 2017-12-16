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

#define NSTRINGS	(sizeof(codes)/sizeof(struct codes))

int debug = 0;
int manual = 0;		/* advance display manually, for debugging */

void
send_command(char *buf) {
	int n;
	int fd = open(X10CTL, O_WRONLY);

	if (fd < 0) {
		perror("open x10ctl file");
		exit(1);
	}
	if (debug >= 2)
		fprintf(stderr, "send_command %s\n", buf);

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
 * Five strings.  Never fewer than 1 or more than 2 strings on.
 */
int
selecttarget(int old) {
	int target;

	while (1) {
		target = random() % (1<<NSTRINGS);
		if (card(target) > 2)
			continue;	/* max two strings, try again */
		if (target == old)
			continue;	/* try again, we must change */
		if (old && ((target & old) == 0))
			continue;	/* must have one old color on */
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
	if (manual)
		exit(0);
	running = 0;
}

void
adjust_lights(int old, int target, int amount, int bright_phase) {
	int j;

	if (debug)
		fprintf(stderr, "%.2x-->%.2x\n", old, target);

	for (j=0; j<NSTRINGS; j++) { /*advance each string*/
		int mask = 1 << j;
		if (((old ^ target) & mask) == 0)
			continue;
		if ((target & mask) && bright_phase) {
			send_command3("bright", j+1, amount);
		} else if ((old & mask) && !bright_phase) {
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
		fprintf(stderr, "...now turn them on, one by one...\n");
	for (i=1; i<=NSTRINGS; i++) {
		if (debug)
			fprintf(stderr, "start %d\n", i);
		send_command2("on", i);
		send_command3("dim", i, 31);
		sleep(2);
	}

#define ADJUST_AMOUNT	4

	while (running) {
		int amount = 0;
		target = selecttarget(old);
		for (amount=0; amount < 32; amount += ADJUST_AMOUNT)
			adjust_lights(old, target, ADJUST_AMOUNT, 0);
		for (amount=0; amount < 32; amount += ADJUST_AMOUNT)
			adjust_lights(old, target, ADJUST_AMOUNT, 1);
		old = target;
		if (running) {
			if (manual) {
				char buf[100];
				printf("Ready (%.02x)...", target);
				if (fgets(buf, sizeof(buf), stdin) == NULL)
					running = 0;
			} else {
				if (debug)
					sleep(10);
				else
					sleep(MINS_BETWEEN_CHANGES*60);
			}
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
	fprintf(stderr, "usage: lights [-d] [-m] [hours]\n");
	return 1;
}
	
int
main(int argc, char *argv[]) {

	ARGBEGIN {
	case 'd': debug++;	break;
	case 'm': manual++;	break;
	case 'Z': ARGF();	break;	// because I like a clean build
	default:
		return(usage());
	} ARGEND;

	if (argc == 1) {
		int limit = atoi(argv[0]);
		if (limit == 0) {	// brief test, usually in november
			alarm(5*60);
		} else {
			alarm(limit*3600);
		}
		signal(SIGALRM, times_up);
	}

	signal(SIGUSR1, times_up);
	signal(SIGHUP, times_up);
	signal(SIGINT, times_up);

	if (manual)
		fprintf(stderr, "Running program in manual\n");
	interesting();
	return 0;
}
