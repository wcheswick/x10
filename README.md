X10 daemon and access programs for Unix/Linux

For more than two decades I have used X10 to control lights and
do various other functions in the home.  I have finally 
installed this software here on the farm, and share it here.

It consists of a daemon with FreeBSD startup file and an x10 access program
for controlling circuits via shell scripts.  Some of these scripts from our
old house are included.

My first programming project on a PDP-8 was "to make the console lights
flash in an interesting manner."  This software extends that assignment
with:

Make Christmas lights change in an interesting and non-tacky manner.

I have five strings of lights, each of a single color, and each running on a
different X10 code.  The tree-lights program turns on at most two colors, and
changes them very slowly.  This is slow enough that people who passed our
house on several nights thought we had put different lights up.  Our
neighbor, Anthony, would should these lights to people who visited, and
asked that I speed up the changes a bit.  Anyway, that code is here.

Also here is a program that transmits Morse code, also very slowly.

I have used LED strings since they became available. Very low power, but the
downside is that the X10 stuff is not able to turn them completely off.
Strings of old-style Christmas lights can pull a lot of amps, and arrangements
are conceivable that would blow the breaker if all five strings are turned on
at once.  I ran the old style for a while, but never had this bug.  Be careful.

Also, for outside lights, it usually becomes necessary to put the controllers
somewhere outside, which they aren't rated for, I am sure.  I have done ok
protecting them in a box in some way.

I welcome other uses.

Installation.

The installation is not quite turnkey, and I don't feel like writing a step-by-step
document.  You need a little system administration skill, and maybe a little C
programming.

My CM11 X10 controller plugs into the wall, and has a serial port.  At present, I
have it hooked up to an older laptop that has a number of functions, and has a
serial port.  So the program is currently hard-coded to use /dev/ttyu0. There are
some lines in the Makefile for using devfs, but that appears to be slightly wrong
at the moment.

There are USB-serial connectors, and this software has worked through one of these
on a Raspberry Pi, though some slight tweaks might be needed.

Both the controller:
	x10
and the client
	x10 on b12
need root access.  They communicate through a named pipe.

The controller is brought up with a startup script.  Logging is appended to /var/log/x10.
A debug flag varies the logging level, and the -D parameter detaches the server into a daemon.


Bill Cheswick
ches@cheswick.com
Dec 2017
