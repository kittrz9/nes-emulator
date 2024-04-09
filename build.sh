#!/bin/sh

[ $CC ] || CC=gcc
[ $NAME ] || NAME="nesEmu"
CFLAGS="$CFLAGS -O2 -Wall -Wextra -Wpedantic -std=gnu99"
LDFLAGS="$LDFLAGS -Wall -Wextra -Wpedantic"
DEFINES="$DEFINES"
LIBS=""

CFILES="$(find src/ -name "*.c")"
OBJS=""

rm -rf build/ obj/

mkdir build/ obj/

for f in $CFILES; do
	OBJNAME="$(echo $f | sed -e "s/\.c/\.o/" -e "s/src/obj/")"
	$CC $CFLAGS -c $f -o $OBJNAME
	OBJS="$OBJS $OBJNAME"
done

$CC $LDFLAGS $OBJS -o build/$NAME $LIBS
