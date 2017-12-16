BIN=/usr/local/bin
RC=/usr/local/etc/rc.d

PROGS=front garage groundfloor mainfloor outside upstairs kitchen sendmorse

CFLAGS = -g -Wall -DDEVNAME='"/dev/ttyu0"'


OBJS=x10.o tty.o cmd.o client.o

all:	x10 tree-lights

x10:	${OBJS}
	${CC} $(CFLAGS) -o x10 ${OBJS} $(LIBS)

tree-lights:	tree-lights.o
	${CC} $(CFLAGS) -o tree-lights tree-lights.o $(LIBS)

lights:	lights.o
	${CC} $(CFLAGS) -o lights lights.o $(LIBS)

lights.o tree-lights.o tungsten-tree-lights.o x10.o:	arg.h
${OBJS}:	x10.h


.for i in ${PROGS}
install::	${BIN}/$i

${BIN}/$i:	bin/$i
	cp $> $@
.endfor

root:: ${BIN}/x10 ${BIN}/tree-lights ${RC}/x10 /dev/x10 ${RC}/x10

install::	${BIN}/x10 ${BIN}/tree-lights 
${BIN}/x10:	x10
	cp x10 $(BIN)/x10
	chown ches $(BIN)/x10
	chmod 4755 $(BIN)/x10

${BIN}/tree-lights:	tree-lights
	cp tree-lights $(BIN)/tree-lights

${RC}/x10startup.sh:	x10startup.sh
	cp $> $@

/dev/x10:
	echo "" >>/etc/devfs.conf
	echo "link	ttyu0	x10" >>/etc/devfs.conf
	echo "own	x10	ches:wheel" >>/etc/devfs.conf
	echo "perm	x10	0666" >>/etc/devfs.conf

clean:
	rm -f *.o x10 lights tree-lights *.core

