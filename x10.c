/*
 * talk to the smarthome controller.  Protocol is miss-documented
 * in http://www.smarthome.com/protocol.txt.  My device appears to
 * be like a CM11.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/signal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>

#include "arg.h"
#include "x10.h"

int debug = 0;
int x10_server;

FILE *logfile = NULL;

int ctl_fd;

#define A2C(a)		((encode[HOUSE(a)]<<4) | (encode[UNIT(a)] & 0x0f))
#define C2A(c)		((decode[(c>>4) & 0xf] << 4) | (decode[c & 0xf]))

/*
 * Tables to translate normal binary addresses to the wretched
 * x10 coding.
 */
uchar encode[] = {
	6, 14, 2, 10, 1, 9, 5, 13,
	7, 15, 3, 11, 0, 8, 4, 12};

uchar decode[] = {
	12, 4, 2, 10, 14, 6, 0, 8,
	13, 5, 3, 11, 15, 7, 1, 9};

#define Poll	0x5a	/* terminal poll, data waiting */
#define TIMEREQ	0xa5	/* what time is it? */

void process_poll_command(void);


static char *
now(void) {
	static char buf[50];
	struct tm *tv;
	time_t t = time(0);

	tv = localtime(&t);
	sprintf(buf, "%d/%02d/%02d %02d:%02d:%02d",
		tv->tm_year + 1900, tv->tm_mon+1, tv->tm_mday,
		tv->tm_hour, tv->tm_min, tv->tm_sec);
	return buf;
}

void 
Log(const char *format, ...) {
	va_list args;
	char buffer[1023];
    
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	fprintf(logfile, "%s %s", now(), buffer);
}

/*
 * Input specificly waits for a character from the x10/rs232
 * interface.
 */	
int
input(int sec) {
	fd_set fdset;
	int n;

	FD_ZERO(&fdset);
	FD_SET(x10port, &fdset);
	if (sec >= 0) {
		struct timeval to;
		to.tv_sec = sec;
		to.tv_usec = 0;
		n = select(x10port+1, &fdset, 0, 0, &to);
	} else
		n = select(x10port+1, &fdset, 0, 0, 0);

	if (n < 0) {
		if (errno == EINTR)
			return 0;
		perror("input, select");
		exit(1);
	}
	if (n == 0)
		return 0;
	return FD_ISSET(x10port, &fdset);
}

#define	RESPONSE_TO	10

int
readbyte(void) {
	uchar ans[1];
	int n;

	if (!input(RESPONSE_TO)) {
		Log("readbyte timed out (%d sec)\n", RESPONSE_TO);
		return -1;
	}

	if ((n = read(x10port, &ans, 1)) != 1) {
		perror("readbyte read");
		exit(1);
	}
	if (debug > 1)
		Log("***> 0x%.2x\n", ans[0]);
	return ans[0];
}

void
eat_ctlr_input(void) {
	int n = 0;
	if (debug > 1)
		Log("eat pending controller input\n");
	while (input(0)) {
		readbyte();
		n++;
	}
	if (debug > 1)
		Log("   bytes eaten: %d\n", n);
}

int
read_expected(unsigned int ack) {
	int ch;

	if (debug > 2)
		Log("read_expected, ack = 0x%.02x\n", ack);
	if ((ch=readbyte()) == ack) {
		if (debug > 1)
			Log("read_expected, got ack\n");
		return 1;
	}
	if (ch == Poll) {
		if (debug > 1)
			Log("read_expected, got Poll\n");
		process_poll_command();
		return 1;
	}
	if (ch < 0)
		Log("read timed out, expecting 0x%.2x\n",
			ack);
	else {
		Log("read_expected 0x%.2x, got 0x%.2x\n",
			ack, ch);
		eat_ctlr_input();
	}
	return 0;
}

unsigned int checksum;

void
timeout(int i) {
	Log("x10 daemon unresponsive, aborting\n");
	exit(1);
}

void
open_logfile(void) {
	if ((logfile=fopen(LOGNAME, "a")) == NULL) {
		perror("fopen error logfile");
		exit(2);
	}
	setbuf(logfile, NULL);
	stderr = logfile;
}

void
restart_log(int i) {
	Log("Restarting log\n");
	fclose(logfile);
	open_logfile();
	Log("New log file\n");
}

void
sendbyte(uchar b) {
	int n;

	alarm(10);

	if (debug > 1)
		Log("<--- 0x%.2x\n", b);
	checksum += b;
	n = write(x10port, &b, 1);
	if (n != 1) {
		perror("sendbyte");
		exit(1);
	}
	alarm(0);
}

void
send(uchar *buf, int len) {
	int i;

	for (i=0; i<len; i++)
		sendbyte(buf[i]);
}

void
stuff(void) {
	int i;

	for (i=0; i<256; i++) {
		if (input(1))
			break;
		sendbyte(i);
	}
}

void
disable_RI(void) {
	if (debug > 1)
		Log("disabling controller Ring Indicator\n");
	sendbyte(0xdb);
	read_expected(0xdb);
	sendbyte(0x00);
	read_expected(0x55);
}

/*
 * For the cm11 controller
 */
void
get_ctlr_status(void) {
	u_char s[112/8];
	int i;

	if (debug > 1)
		Log("get_ctlr_status CM11\n");
	sendbyte(0x8b);
	for (i=0; i<sizeof(s); i++) {
		int b = readbyte();
		if (b < 0)
			return;
		else
			s[i] = b & 0xff;
	}
	if (debug)
	Log("status: batt timer %.04x s:%d  m:%d  h:%d  yd:%d  dowmask:%.02x  mon: %.01X  firm: %.01X  mon %.04x %.04x %.04x\n",
			(s[0]<<8) + s[1] , s[2], s[3], s[4],
			((s[6]&0x80)<<1) + s[5], s[6] & 0x7f,
			(C2A(s[7])>>4) & 0x0f, s[7] & 0x0f,
			(s[8]<<8) + s[9], (s[10]<<8) + s[11], (s[12]<<8) + s[13]);
}

void
sendmsg(int b1, int b2) {
	do {
		checksum = 0;
		sendbyte(b1);
		sendbyte(b2);
	} while (!read_expected(checksum & 0xff));
	sendbyte(0x00);
	read_expected(0x55);
}

void
sendemsg(int b1, int b2, int b3, int b4, int b5) {
	do {
		sendbyte(b1);
		checksum = 0;
		sendbyte(b2);
		sendbyte(b3);
		sendbyte(b4);
		sendbyte(b5);
		sendbyte(0);
	} while (!read_expected(checksum & 0xff));
	sendbyte(0x00);
	read_expected(0x55);
}


#define Addr	0x04
#define Func	0x02
#define Ext	0x01

void
on(addr a) {
	int addrc = A2C(a);

	sendmsg(Addr, addrc);
	sendmsg(Func|Addr, (addrc & 0xf0) | On);
}

void
off(addr a) {
	int addrc = A2C(a);

	sendmsg(Addr, addrc);
	sendmsg(Func|Addr, (addrc & 0xf0) | Off);
}

void
dim(addr a, int v) {
	int addrc = A2C(a);

	sendmsg(Addr, addrc);
	sendmsg(((v & 0x1f) << 3) | Func | Addr, (addrc & 0xf0) | Dim);
}

void
brighten(addr a, int v) {
	int addrc = A2C(a);

	sendmsg(Addr, addrc);
	sendmsg(((v & 0x1f) << 3) | Func | Addr, (addrc & 0xf0) | Bright);
}

void
alloff(addr a) {
	int addrc = A2C(a);

	sendmsg(Addr, addrc);
	sendmsg(Func|Addr, (addrc & 0xf0) | AllOff);
}

void
extended(struct cmd c) {
	sendemsg((Ext<<4) | Ext,
		c.p[0], c.p[1], c.p[2], c.p[3]);
}

void
allon(addr a) {
	int addrc = A2C(a);

	sendmsg(Addr, addrc);
	sendmsg(Func|Addr, (addrc & 0xf0) | AllOn);
}

void
answer_timereq(void) {
	struct tm *t;
	time_t c = time(0);
	int jdate;
	uchar ans;

	t = localtime(&c);
	jdate = t->tm_yday + 1;
	sendbyte(0x9b);	/* timer download header */
	checksum = 0;
	sendbyte(t->tm_sec);
	sendbyte(t->tm_min + 60*(t->tm_hour % 1));
	sendbyte(t->tm_hour/2);
	sendbyte(jdate % 256);
	sendbyte((((jdate / 256) & 0x01)<<7) | t->tm_wday);
	sendbyte(A2C(0));	/* Monitored house code.  For what? */

	while ((ans=readbyte()) == TIMEREQ)
		;
	if (ans != (checksum & 0xff)) {
		Log("answer_timereq wrong answer 0x%.2x, got 0x%.2x\n",
			(checksum & 0xff), ans);
		return;	
	}
#ifdef notdef
// not sure what this was for, but doesn't happen
	sendbyte(0x00);
	read_expected(0x55);
#endif
}

#define NQUEUE	5
int nqueue = 0;
char *queue[NQUEUE];

void
queue_command(char *cmd) {
	if (nqueue == NQUEUE) {
		Log("%s command queue full (%s)\n", cmd);
		return;
	}
	queue[nqueue++] = cmd;
}

void
run_queue(void) {
	int i;

	for (i=0; i<nqueue; i++)
		switch (fork()) {
		case -1:
			perror("fork");
			exit(2);
		case 0:
			if (setsid() < 0) {
				perror("setsid");
				exit(1);
			}
			system(queue[i]);
			exit(0);
		}
	nqueue = 0;
}

/*
 * We were polled, data is available.  Get it.
 */
struct cmd last_cmd;

void
process_poll_command(void) {
	uchar response[256];
	int i, n;

	sendbyte(0xc3);

	n = readbyte();
	if (n < 0)
		return;
	for (i=0; i<n; i++) {
		int r = readbyte();
		if (r < 0)
			return;
		response[i] = r;
	}
	if (debug) {
		Log("%d bytes to poll:\n", n);
		Log("function/address mask: %.02x\n", response[0]);
		for (i=1; i<n; i++)
			Log("  %.02x\n", response[i]);
	}

	for (i=1; i<n; i++) {
		if ((response[0] >> ((i-1)) & 0x01)) {/* function */
			int h, u;

			last_cmd.c = response[i] & 0xf;
			switch (last_cmd.c) {
			case Bright:
			case Dim:
				last_cmd.p[0] = response[++i];
				break;
			case ExtendedCode:
				last_cmd.p[0] = response[i++]; /* 0x07 */
				last_cmd.p[1] = response[i++];
				last_cmd.p[1] = 0;	// ignore this unknown field
				last_cmd.p[2] = response[i++];
				last_cmd.p[3] = response[i++];
				h = decode[last_cmd.p[0] >> 4];
				switch ((last_cmd.p[2] >> 4) & 0xf) {
				case 1:	u = 1; break;
				case 2:	u = 2; break;
				case 4:	u = 3; break;
				default:
					Log("extended unit error: 0x%.02x 0x%.02x\n",
						last_cmd.p[0], last_cmd.p[2]);
					u = 0;
				}
				last_cmd.a = (h << 4) | (u-1);
				break;
			}
			execute_x10_signal(last_cmd);
		} else {
			memset(&last_cmd, 0, sizeof(last_cmd));
			last_cmd.a = C2A(response[i]);
			if (debug)
				Log("%d: 0x%.02x 0x%.02x\n",
					i, response[i], last_cmd.a);
		}
	}
}

/*
 * Check the x10 serial port for incoming messages.  If they
 * answer our poll, check the command supplied.
 */
void
check_poll(void) {
	int ans;

	if (debug > 1)
		Log("check_poll\n");
	if (!input(0))
		return;
	ans = readbyte();
	if (debug > 1)
		Log("check_poll: input 0x%.02x\n", ans);
	switch (ans) {
	case 0xa5:
		Log("* setting time\n");
		answer_timereq();
		break;
	case Poll:
		process_poll_command();
		break;
	case -1:	/* read timed out */
		break;
	default:
		Log("check_poll: unexpected poll! 0x%.2x\n",
			ans);
	}
}

int
init_ctl_fifo(void) {
	int i;
	struct stat s;

	if (debug > 1)
		Log("init_ctl_fifo: checking %s\n", X10CTL);
	if (stat(X10CTL, &s) < 0) {
		if (errno == ENOENT) {
			i = mkfifo(X10CTL, 0666);
			if (i < 0) {
				perror("mkfifo");
				return 0;
			}
			chmod(X10CTL, 0666);
		}
	}

	if (debug > 1)
		Log("init_ctl_fifo: opening %s\n", X10CTL);
	ctl_fd = open(X10CTL, O_NDELAY|O_NONBLOCK);
	if (ctl_fd < 0) {
		perror("open ctl");
		return 0;
	}
	if (fcntl(ctl_fd, F_SETFL, 0) < 0) {
		perror("open fcntl");
		exit(1);
	}
	return 1;
}

int
new_config(char *cf, time_t last_check) {
	struct stat sb;

	if (stat(cf, &sb) < 0) {
		char buf[100];
		snprintf(buf, sizeof(buf),
			 "x10: can't stat config file %s: %s\n",
			 cf, strerror(errno));
		return 0;
	}

	return (sb.st_mtime > last_check);
}

int
usage(void) {
	fprintf(logfile, "usage: x10 [-d] [command]\n");
	return 1;
}
	
int
main(int argc, char *argv[]) {
	char *cf = CONFFN;
	char *pidfile = NULL;
	time_t last_config_check = 0;
	int be_daemon = 0;

	logfile = stderr;

	ARGBEGIN {
	case 'D': be_daemon=1;	break;
	case 'd': debug++;	break;
	case 'f': cf = ARGF();	break;
	case 'P': pidfile = ARGF();
				break;
	default:
		return usage();
	} ARGEND;

	/*
	 * If commands are present, we are the client.  We send commands
	 * to the X10 server through a named pipe.
	 */
	if (argc)	
		return do_client(argc, argv);

	if (be_daemon) {
		if (daemon(0, 0)) {
			perror("daemon failure");
			exit(1);
		}
		open_logfile();
		Log("Starting\n");
	}

	if (pidfile) {
		FILE *pid = fopen(pidfile, "w");
		if (pid == NULL) {
			perror("open PID file, aborting");
			exit(1);
		}
		fprintf(pid, "%d", getpid());
		fclose(pid);
	}

	if (!read_config(cf))
		return 1;
	last_config_check = time(0);

	if (!init_ctl_fifo())
		return 1;
	
	signal(SIGALRM, timeout);
	signal(SIGHUP, restart_log);
	if (debug > 1)
		Log("set up tty to x10 controller\n");
	setup_tty();

	if (input(3)) {	// see if he is polling us
		if (debug > 1)
			Log("He is awake and polling us\n");
		check_poll();
	}

	get_ctlr_status();
	disable_RI();

	while (1) {
		/*
		 * Check and process configuration file changes
		 */
		time_t t = time(0);
		if (t > last_config_check + CONFCHKTIME) {
			if (new_config(cf, last_config_check))
				read_config(cf);
			last_config_check = t;
		}

		/*
		 * check for a poll from the X10 controller
		 */
		if (input(1))
			check_poll();

		/*
		 * Check the pipe for pending commands.  Contains a non-blocking
		 * read.
		 */
		do_pipe_commands(ctl_fd);

		/*
		 * check and drain pending queue of commands
		 */
		run_queue();
		waitpid(-1, 0, WNOHANG);
	}
}
