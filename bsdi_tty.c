
#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <fcntl.h>

#include "x10.h"

#define	DEVNAME	"/dev/x10"

int x10port = -1;
struct termios
    oldsb,
    newsb;

void
set_blocking(int on) {
	if (on) {
		if (fcntl(x10port, F_SETFL, 0) < 0)
			perror("set_blocking: fcntl 1");
	} else {
		if (fcntl(x10port, F_SETFL, O_NONBLOCK) < 0)
			perror("set_blocking: fcntl 2");
	}

}

void
setup_tty(void) {
	x10port = open(DEVNAME, O_RDWR|O_NOCTTY|O_NONBLOCK, 0666);
	if (x10port < 0) {
		perror(DEVNAME);
		exit();
	}

	tcgetattr(x10port, &oldsb);
	newsb = oldsb;
	cfsetispeed(&newsb, B4800);
	cfsetospeed(&newsb, B4800);
	cfmakeraw(&newsb);
	newsb.c_cflag |= CLOCAL|CREAD;
/*
	newsb.c_cc[VEOF] = 1;
	newsb.c_cc[VEOL] = 0;
*/
	if (tcsetattr(x10port, TCSANOW, &newsb) < 0)
		perror("setup_tty");
	set_blocking(1);
fprintf(stderr, "setup_tty: %s ready\n", DEVNAME);
}
