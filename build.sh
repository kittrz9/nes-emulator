#!/bin/sh

set -xe

SDL_VERSION="3.4.0"

if [ "$DEBUG" ]; then
	#DEFINES="-DDEBUG -DUNCAP_FPS"
	CFLAGS="-g -fsanitize=address"
	LDFLAGS="-g -fsanitize=address"
fi

[ "$CC" ] || CC=gcc
[ "$NAME" ] || NAME="nesEmu"
CFLAGS="$CFLAGS -g -ISDL3-$SDL_VERSION/include/ -O2 -Wall -Wextra -Wpedantic -std=c99"
LDFLAGS="$LDFLAGS -Wall -Wextra -Wpedantic"
DEFINES="$DEFINES"
# I'm probably not using rpath correctly lmao
LIBS="-Wl,-rpath=./ -Wl,-rpath=build/ -L./build/ -lSDL3 "

CFILES="$(find src/ -name "*.c")"
OBJS=""

if ! [ -d "SDL3-$SDL_VERSION" ]; then
	if ! [ -f "SDL3-$SDL_VERSION.tar.gz" ]; then
		wget "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-$SDL_VERSION.tar.gz"
	fi
	tar -xavf SDL3-$SDL_VERSION.tar.gz
fi

if ! [ -f "SDL3-$SDL_VERSION/build/libSDL3.so.0" ]; then
	ORIGIN_DIR="$(pwd)"
	cd "$ORIGIN_DIR/SDL3-$SDL_VERSION"
	cmake -S . -B build
	cmake --build build -j "$(nproc)"
	cd "$ORIGIN_DIR"
fi

rm -rf build/ obj/

mkdir build/ obj/

# idk why I specifically need this file instead of just libSDL2.so
cp SDL3-$SDL_VERSION/build/libSDL3.so SDL3-$SDL_VERSION/build/libSDL3.so.0 build/

for f in $CFILES; do
	OBJNAME="$(echo "$f" | sed -e "s/\.c/\.o/" -e "s/src/obj/")"
	$CC $CFLAGS $DEFINES -c "$f" -o "$OBJNAME" &
	OBJS="$OBJS $OBJNAME"
done

wait

$CC $LDFLAGS $OBJS -o build/$NAME $LIBS
