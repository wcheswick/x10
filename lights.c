/*
 * This program issues X10 commands to four strings of Christmas
 * lights.  Our four strings are twined on two light posts in front
 * of our house.
 *
 * We implement not some tacky flashing thing, but use the X10 dimming
 * function to slowly switch colors.  If you aren't watching closely,
 * you will miss the slow changes between various states.  Some people
 * driving by have noticed the different colors, and assumed that we
 * actually changed lights on the poles!
 *
 * Of course, the same X10 signals could go straight to a Christmas tree.
 * It takes a lot of lights, because you have to decorate the tree four
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

/*
 * This simple code assumes four unit codes in a row, starting as defined.
 * We assume two light poles, with two complimentary colors on each.
 * I use yellow/bluw on one and red/green on the other.
 */

#define	HOUSE_CODE	'd'
#define BASE_UNIT	13

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

	sprintf(buf, "%s %c%d", cmd, HOUSE_CODE, BASE_UNIT+l-1);
	send_command(buf);
}

void
send_command3(char *cmd, int l, int c) {
	char buf[100];

	sprintf(buf, "%s %c%d %d", cmd, HOUSE_CODE, BASE_UNIT+l-1, c);
	send_command(buf);
}

/*
 * determine our next pattern.  We have esthetic constraints.
 * Strings 0 and 1 are on one post, 2 and 3 on the other.
 */
int
selecttarget(int old) {
	int target;

	while (1) {
		target = random() & 0xf;
		/*
		 * It must change.
		 */
		if (target == old) /* we must change */
			continue;
		/*
		 * Each post must have at least one string on.
		 */
		if (!(target & 0x3) || !(target & 0xc))
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
	int j, waittime = 0;

	for (j=0; j<4; j++) { /*advance each string*/
		int mask = 1 << j;
		if (((old ^ target) & mask) == 0)
			continue;
		if (target & mask) {
			send_command3("bright", j+1, amount);
			waittime += (amount/5) + 1;
		} else {
			send_command3("dim", j+1, amount);
			waittime += (amount/5) + 1;
		}
	}
	sleep(waittime);	/* let the dims execute */
}

void
interesting(void) {
	int target, old = 0xf;	/*start with all four strings on */
	int i;

	srandom(time(0));
	send_command2("on", 1);	/* start with the four on */
	send_command3("bright", 1, 30);
	sleep(1);
	send_command2("on", 3);	/*staggered start, for interest */
	send_command3("bright", 3, 30);
	sleep(1);
	send_command2("on", 2);
	send_command3("bright", 2, 30);
	sleep(1);
	send_command2("on", 4);
	send_command3("bright", 4, 30);
	sleep(1);

	while (running) {
		target = selecttarget(old);
		for (i=0; i<20; i++)
			adjust_lights(old, target, 2);
		adjust_lights(old, target, 10);	/* make sure they made it */
		old = target;
		sleep(240);	/* pause */
	}
	send_command2("off", 1);
	sleep(1);
	send_command2("off", 3);
	sleep(1);
	send_command2("off", 2);
	sleep(1);
	send_command2("off", 4);
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
		
	interesting();
	return 0;
}
