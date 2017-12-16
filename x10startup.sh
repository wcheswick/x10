#!/bin/sh
#

# PROVIDE: x10
# REQUIRE: DAEMON

# Add the following line to /etc/rc.conf to enable 'x10'
#
#x10_enable="YES"
#

. "/etc/rc.subr"

PATH=$PATH:/usr/local/bin:/home/ches/bin;	export PATH

name="x10"
rcvar=`set_rcvar`
pidfile="/var/run/$name/pid"
command="/usr/local/bin/x10"
command_args="-D -P $pidfile"

# read configuration and set defaults
load_rc_config "$name"
: ${x10_enable="NO"}
: ${x10_flags=""}

if [ ! -d /var/run/$name ]
then
	mkdir -m 0775 -p /var/run/$name
	chown ches:wheel /var/run/$name
fi

run_rc_command "$1"
