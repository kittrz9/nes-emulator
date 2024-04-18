#!/bin/sh

set -xe

SDL_VERSION="2.30.2"

[ $CC ] || CC=gcc
[ $NAME ] || NAME="nesEmu"
CFLAGS="$CFLAGS -I./SDL2-$SDL_VERSION/include/ -O2 -Wall -Wextra -Wpedantic -std=c99"
LDFLAGS="$LDFLAGS -Wall -Wextra -Wpedantic"
DEFINES="$DEFINES"
# I'm probably not using rpath correctly lmao
LIBS="-Wl,-rpath=./ -L./build/ -lSDL2 "

CFILES="$(find src/ -name "*.c")"
OBJS=""

if ! [ -d "SDL2-$SDL_VERSION" ]; then
	if ! [ -f "SDL2-$SDL_VERSION.tar.gz" ]; then
		wget "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz"
	fi
	tar -xavf SDL2-$SDL_VERSION.tar.gz
fi

if ! [ -f "SDL2-$SDL_VERSION/build/.libs/libSDL2-2.0.so.0" ]; then
	ORIGIN_DIR="$(pwd)"
	cd "$ORIGIN_DIR/SDL2-$SDL_VERSION"
	./configure
	make -j$(nproc)
	cd "$ORIGIN_DIR"
fi

rm -rf build/ obj/

mkdir build/ obj/

# idk why I specifically need this file instead of just libSDL2.so
cp SDL2-$SDL_VERSION/build/.libs/libSDL2-2.0.so.0 build/

for f in $CFILES; do
	OBJNAME="$(echo $f | sed -e "s/\.c/\.o/" -e "s/src/obj/")"
	$CC $CFLAGS $DEFINES -c $f -o $OBJNAME &
	OBJS="$OBJS $OBJNAME"
done

wait

$CC $LDFLAGS $OBJS -o build/$NAME $LIBS
