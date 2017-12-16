
#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <fcntl.h>

#include "x10.h"

#define	DEVNAME	"/dev/x10"

int tty = -1;
struct termios
    oldsb,
    newsb;

void
setup_tty(void) {
	tty = open(DEVNAME, O_RDWR|O_NOCTTY|O_NONBLOCK, 0666);
	if (tty < 0) {
		perror(DEVNAME);
		exit();
	}

	tcgetattr(tty, &oldsb);
	newsb = oldsb;
	cfsetspeed(&newsb, B4800);
	cfmakeraw(&newsb);
	newsb.c_cflag |= CLOCAL|CREAD;
/*
	newsb.c_cc[VEOF] = 1;
	newsb.c_cc[VEOL] = 0;
*/
	if (tcsetattr(tty, TCSANOW, &newsb) < 0)
		perror("setup_tty");
fprintf(stderr, "%s opened and set\n", DEVNAME);
	if (fcntl(tty, F_SETFL, 0) < 0)
		perror("setup_tty: fcntl");
}
