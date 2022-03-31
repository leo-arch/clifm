#!/bin/sh

# Image thumbnails plugin for CliFM
# Written by L. Abramovich
# Dependencies: sxiv | feh | lsix | img2sixel
# License: GPL3

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Display thumbails of FILE(s) or files in DIR (current \
working directory if omitted).
Usage: %s [FILE... n] [DIR]\n" "$name"
	exit 0
fi

found=0
args="${*:-.}"

if type sxiv > /dev/null 2>&1; then
	sxiv -aqt -- "$args" && exit 0 || found=1
elif type feh > /dev/null 2>&1; then
	feh -tZ -- "$args" && exit 0 || found=1
elif type lsix > /dev/null 2>&1; then
	if [ -d "$1" ] || [ -h "$1" ]; then
		lsix "$1"/*.png "$1"/*.jpg && exit 0 || found=1
	else
		lsix "$args" && exit 0 || found=1
	fi
elif type img2sixel > /dev/null 2>&1; then
	img2sixel "$args" && exit 0 || found=1
fi

if [ "$found" -eq 0 ]; then
	printf "CliFM: No image viewer found\n" >&2
fi
exit 1
