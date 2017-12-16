#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "x10.h"

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

/*
 * Make a pre-existing termios structure into "raw" mode: character-at-a-time
 * mode with no characters interpreted, 8-bit data path.  From BSD.
 */
void
cfmakeraw(struct termios *t) {
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;		/* ??? */
	t->c_cc[VTIME] = 0;		/* ??? */
}

void
setup_tty(void) {
	x10port = open(DEVNAME, O_RDWR|O_NOCTTY|O_NONBLOCK, 0666);
	if (x10port < 0) {
		perror(DEVNAME);
		exit(1);
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
}
