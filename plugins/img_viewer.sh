#!/bin/sh

# Image thumbnails plugin for CliFM
# Written by L. Abramovich
# License: GPL3

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "help" ]; then
	name="$(basename "$0")"
	printf "Display thumbails of FILE(s) or of files in DIR. Use '*' to preview \
all image files in the current directory.
Usage: %s [FILE... n] [DIR]\n" "$name"
	exit 0
fi

"$CLIFM_IMG_VIEWER" "$@" 2>/dev/null && exit 0
sxiv -aqt -- "$@" 2>/dev/null && exit 0
feh -tZ -- "$@" 2>/dev/null && exit 0

if type lsix > /dev/null 2>&1; then
	if [ -d "$1" ] || [ -h "$1" ]; then
		lsix "$1"/*.png "$1"/*.jpg && exit 0
	else
		lsix "$@" && exit 0
	fi
elif type img2sixel > /dev/null 2>&1; then
	img2sixel "$@" && exit 0
fi

printf "CliFM: No image viewer found\n" >&2
exit 1
