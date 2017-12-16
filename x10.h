#ifndef DEVNAME
#define	DEVNAME	"/dev/x10"
#endif

#ifndef LOGNAME
#define	LOGNAME	"/var/log/x10"
#endif

#define X10CTL	"/var/run/x10/ctl"

#define X10DIR	"/home/ches/git/x10"

#define CONFFN	"/home/ches/git/x10/x10.conf"
#define CONFCHKTIME	20

#define HOUSE(a)	(((a)>>4) & 0xf)
#define UNIT(a)		((a) & 0xf)

enum {
	AllOff,
	AllOn,
	On,
	Off,
	Dim,
	Bright,
	AllLightsOff,
	ExtendedCode,
	HailReq,
	HailAck,
	PresetDim1,
	PresetDim2,
	ExtDataTrans,
	StatusOn,
	StatusOff,
	StatusReq,
	Error
};

typedef unsigned char uchar;
typedef unsigned int addr;

struct cmd {
	addr	a;
	uchar	c;
	uchar	p[8];
};

/* x10.c */
extern	int debug;
extern	uchar encode[];
extern	uchar decode[];
extern	FILE *logfile;
extern	void  Log(const char *format, ...);

extern	void on(addr a);
extern	void off(addr a);
extern	void dim(addr a, int v);
extern	void brighten(addr a, int v);
extern	void alloff(addr a);
extern	void allon(addr a);
extern	void extended(struct cmd);
extern	char *show_addr(addr a);
extern	void queue_command(char *cmd);

/* client.c */
extern	int do_client(int argc, char *argv[]);

/* tty.c */
extern	void setup_tty(void);
extern	int x10port;
extern	void set_blocking(int);

/* cmd.c */
extern	char *cmds[];
extern	void do_pipe_commands(int fd);
extern	int read_config(char *fn);
extern	void execute_x10_signal(struct cmd c);
