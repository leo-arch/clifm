#!/bin/env bash

# Wallpaper setter plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

WALLSETTER="feh"
OPTS="--no-fehbg --bg-fill"

if [[ -z "$1" ]]; then
	echo "CliFM: Missing argument" >&2
	exit $ERROR
fi

if ! [[ $(type -P file)  ]]; then
	echo "CliFM: file: Command not found" >&2
	exit $ERROR
fi

if ! [[ $(type -P "$WALLSETTER") ]]; then
	echo "CliFM: ${WALLSETTER}: Command not found" >&2
	exit $ERROR
fi

if ! [[ $(file -bi "$1" | grep "image/") ]]; then
	echo "CliFM: ${1}: Not an image file" >&2
	exit $ERROR
fi

"$WALLSETTER" $OPTS "$1"

if [[ $? == 0 ]]; then
	exit $SUCCESS
fi

exit $ERROR
