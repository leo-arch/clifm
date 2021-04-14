#!/bin/sh

# Wallpaper setter plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

WALLSETTER="feh"
OPTS="--no-fehbg --bg-fill"

if [ -z "$1" ]; then
	printf "CliFM: Missing argument\n" >&2
	exit $ERROR
fi

if ! [ "$(which file 2>/dev/null)" ]; then
	printf "CliFM: file: Command not found\n" >&2
	exit $ERROR
fi

if ! [ "$(which "$WALLSETTER" 2>/dev/null)" ]; then
	printf "CliFM: %s: Command not found\n" "$WALLSETTER" >&2
	exit $ERROR
fi

if ! [ "$(file -bi "$1" | grep "image/")" ]; then
	printf "CliFM: %s: Not an image file\n" "$1" >&2
	exit $ERROR
fi

"$WALLSETTER" $OPTS "$1"

if [ $? -eq 0 ]; then
	exit $SUCCESS
fi

exit $ERROR
