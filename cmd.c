#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "x10.h"

#define MAXTBL	100

int ntbl;
struct tbl {
	struct cmd	c;
	char 	label[100];
	char	exec[100];
} tbl[MAXTBL];

char *cmds[] = {
	"alloff",
	"allon",
	"on",
	"off",
	"dim",
	"bright",
	"alllightsoff",
	"extendedcode",
	"hailreq",
	"hailack",
	"presetdim1",
	"presetdim2",
	"extdatatrans",
	"statuson",
	"statusoff",
	"statusreq",
	0
};

int lineno = 0;
int error;
char line[400];

void
showerror(char *msg) {
	if (lineno == 0)
		Log("'%s' error: %s\n", line, msg);
	else
		Log("line %2d: '%s' error: %s\n", lineno, line, msg);
	error++;
}

static char *
show_x10_event(struct cmd c) {
	static char buf[200];

	switch (c.c) {
	case AllOn:
	case AllOff:
		sprintf(buf, "%s %c", cmds[c.c], HOUSE(c.a)+'a');
		break;
	case Bright:
	case Dim:
		sprintf(buf, "%s %c%d %d",
			cmds[c.c], HOUSE(c.a)+'a', UNIT(c.a)+1, c.p[0]);
		break;
	case ExtendedCode:
		sprintf(buf, "%s %c%d %s (%.02x %.02x %.02x %.02x)",
			cmds[c.c], HOUSE(c.a)+'a', UNIT(c.a)+1,
			c.p[2] & 0x1 ? "on" : "off",
			c.p[0], c.p[1], c.p[2], c.p[3]);
		break;
	case On:
	case Off:
	default:
		sprintf(buf, "%s %c%d", cmds[c.c], HOUSE(c.a)+'a', UNIT(c.a)+1);
	}
	return buf;
}

void
execute_x10_signal(struct cmd c) {
	int ci;

	if (debug > 3)
		Log("execute command: %s\n",
			show_x10_event(c));

	for (ci=0; ci<ntbl; ci++)
		if (memcmp(&c, &tbl[ci].c, sizeof(struct cmd)) == 0)
			break;
	if (ci == ntbl) {
		Log("%s (unprocessed)\n", show_x10_event(c));
		return;
	}

	Log("%s   %s (%s)\n",
		show_x10_event(c), tbl[ci].label, tbl[ci].exec);
	if (tbl[ci].exec[0] != '\0')
		queue_command(tbl[ci].exec);
}

static int
getaddr(char *cp, addr *a, int house_only) {
	char c = *cp;
	int n;

	if (isupper(c))
		c = tolower(c);
	if (c < 'a' || c > 'a'+15) {
		showerror("illegal house code");
		return 0;
	}

	if (house_only)
		n = 1;
	else {
		n = atoi(cp+1);
		if (n < 1 || n > 16) {
			showerror("illegal unit number");
			return 0;
		}
	}
	*a = ((c-'a') << 4) | (n-1);
	return 1;
}

/*
 * Crack a command.  It is terminated by a null or space.  
 */
static int
crack_x10_command(char *cp, struct cmd *c) {
	int i, unit;
	unsigned int p1;
	char s1[10];
	char *ep;
	char addr[20];

	memset(c, 0, sizeof(struct cmd));
	ep = strchr(cp, ' ');
	if (ep)
		*ep++ = '\0';

	/*
	 * Look up command.
	 */
	for (i=0; cmds[i]; i++)
		if (strcasecmp(cmds[i], cp) == 0)
			break;
	c->c = i;
	cp = ep;

	switch (c->c) {
	case AllOff:
	case AllOn:
		if (!cp || (i = sscanf(cp, "%s", addr)) != 1) {
			showerror("usage: allon|alloff <house-code>");
			return 0;
		}
		if (!getaddr(addr, &c->a, 1))
			return 0;
		break;
	case On:
	case Off:
		if (!cp || (i = sscanf(cp, "%s", addr)) != 1) {
			showerror("usage: on|off <x10-addr>");
			return 0;
		}
		if (!getaddr(addr, &c->a, 0))
			return 0;
		break;
	case Dim:
	case Bright:
		if ((i = sscanf(cp, "%s %d", addr, &p1)) != 2) {
			showerror("usage: dim|bright <x10-addr> <value>");
			return 0;
		}
		if (!getaddr(addr, &c->a, 0))
			return 0;
		c->p[0] = p1;
		break;
	case ExtendedCode:
#ifdef notdef
		if ((i = sscanf(cp, "%s %s %s", addr, s1, s2)) != 3) {
			showerror("usage: extendedcode <x10-addr> <hex> <hex>");
			return 0;
		}
		if (!getaddr(addr, &c->a, 0))
			return 0;
		c->p[0] = strtol(s1, 0, 0);
		c->p[1] = strtol(s2, 0, 0);
#endif
		if ((i = sscanf(cp, "%s %s", addr, s1)) != 2) {
			showerror("usage: extendedcode <x10-addr> [on|off]");
			return 0;
		}
		if (!getaddr(addr, &c->a, 0))
			return 0;
		unit = (UNIT(c->a)) + 1;
		if (unit < 1 || unit > 3) {
			showerror("extendedcode unit must be 1, 2, or 3");
			return 0;
		}
		i = 1 << (unit-1);

		c->p[0] = (encode[HOUSE(c->a)] << 4) | (ExtendedCode);
		c->p[1] = 0x07;		/* always, I don't know why */
		c->p[1] = 0x01;		/* new always, I don't know why */
		c->p[1] = 0;		/* screw it, we don't care */
		if (strcmp(s1, "on") == 0)
			c->p[2] = (i<<4) | i | 1;
		else if (strcmp(s1, "off") == 0)
			c->p[2] = (i<<4);
		else {
			showerror("usage: extendedcode <x10-addr> [on|off]");
			return 0;
		}
		c->p[3] = 0x1d;		/* always, don't know why */
		break;
	case Error:
		showerror("unknown command");
		return 0;
	default:
		showerror("unimplemented command");
		return 0;
	}
	return 1;
}

int
read_config(char *fn) {
	FILE *f;
	char buf[400];
	char *cp;
	struct tbl newtbl[MAXTBL];
	int nnewtbl = 0;


	f = fopen(fn, "r");
	if (f == 0) {
		if (errno == ENOENT)
			return 1;
		perror("open config file");
		Log("configuration unchanged.\n");
		return 0;
	}

	lineno = 0;  error = 0;
	Log("reading config file %s\n", fn);
	while (fgets(buf, sizeof(buf), f)) {
		lineno++;

		if (buf[0] == '#' || buf[0] == ' ' ||
		    buf[0] == '\t' || buf[0] == '\n')
			continue;
		cp = strchr(buf, '\n');
		if (cp)
			*cp = '\0';
		strncpy(line, buf, sizeof(line)-1);

		/*
		 * crack the X10 event description.
		 */
		if ((cp = strtok(buf, "\t")) == 0)
			continue;
		if (!crack_x10_command(cp, &newtbl[nnewtbl].c))
			continue;
		if (debug > 3)
			Log("parsed command: %s\n",
				show_x10_event(newtbl[nnewtbl].c));

		/*
		 * crack the label for this event.
		 */
		if ((cp = strtok(NULL, "\t")) == 0)		/* label */
			continue;
		strncpy(newtbl[nnewtbl].label, cp, sizeof(newtbl[nnewtbl].label)-1);

		/*
		 * get the shell command we execute if we see this event.
		 */
		if ((cp = strtok(NULL, "\t")) != 0)		/* cmd */
			strncpy(newtbl[nnewtbl].exec, cp,
				sizeof(newtbl[nnewtbl].exec)-1);
		else
			newtbl[nnewtbl].exec[0] = '\0';
		if (nnewtbl == MAXTBL) {
			showerror("too many configuration entries");
			break;
		}
		nnewtbl++;
	}
	fclose(f);
	lineno = 0;
	if (error) {
		if (ntbl == 0)
			Log("configuration errors, aborting.\n");
		else
			Log("configuration errors, not changed.\n");
		return 0;
	}
	memcpy(tbl, newtbl, sizeof(tbl));
	ntbl = nnewtbl;
	Log("configuration updated.\n");
	return 1;
}


/*
 * Read a command from the pipe.  Commands are zero-byte or newline
 * terminated.  We don't assume that reads from the pipe are delimited,
 * which would be nice, but some systems don't seem to want to.
 *
 * NB: The read is non-blocking.
 */
static int
read_pipe_command(int fd, char *buf, int len) {
	int i=0, n;

	if (debug > 2)
		Log("read_pipe_command: reading %d\n", len);

	for (i=0; i<len; i++) {
		n = read(fd, &buf[i], 1);
		if (n <= 0)
			return n;
		if (buf[i] == '\n')
			buf[i] = '\0';
		if (buf[i] == '\0')
			return i;
	}
	buf[len] = '\0';
	return len;
}

/*
 * Process commands from the x10 program through the named pipe.
 */
void
do_pipe_commands(int fd) {
	char buf[100];
	struct cmd c;
	int n;

	while ((n=read_pipe_command(fd, buf, sizeof(buf))) > 0) {
		if (n < 0) {
			perror("read ctl");
			exit(1);
		}
		strcpy(line, buf);

		if (!crack_x10_command(buf, &c))
			continue;
		Log("-%s\n", show_x10_event(c));

		switch (c.c) {
		case On:
			on(c.a);
			break;
		case Off:
			off(c.a);
			break;
		case Dim:
			dim(c.a, c.p[0]);
			break;
		case Bright:
			brighten(c.a, c.p[0]);
			break;
		case AllOff:
			alloff(c.a);
			break;
		case AllOn:
			allon(c.a);
			break;
		case ExtendedCode:
			extended(c);
			break;
		default:
			showerror("Command unimplemented");
		}
	}
}
