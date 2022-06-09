#!/bin/sh

# Image thumbnails plugin for CliFM
# Written by L. Abramovich
# Dependencies (any of the following):
#     sxiv feh pqiv gthumb ristretto gwenview lsix img2sixel
#     or just your preferred image viewer
# License: GPL3

# Specify here your preferred image viewer and command line options for it
VIEWER=""
VIEWER_OPTS=""

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Display thumbails of FILE(s) or files in DIR (current \
working directory if omitted).
Usage: %s [FILE... n] [DIR]\n" "$name"
	exit 0
fi

found=0
args="${*:-.}"

if [ -n "$VIEWER" ] && [ "$(type "$VIEWER" 2>/dev/null)" ]; then
	if [ -n "$VIEWER_OPTS" ]; then
		"$VIEWER" "$VIEWER_OPTS" "$args"
	else
		"$VIEWER" "$args"
	fi
	exit 0
fi

if type sxiv > /dev/null 2>&1; then
	sxiv -aqt -- "$args" && exit 0 || found=1
elif type feh > /dev/null 2>&1; then
	feh -tZ -- "$args" && exit 0 || found=1
elif type pqiv > /dev/null 2>&1; then
	pqiv --auto-montage-mode --max-depth=1 --disable-backends="archive,poppler,spectre,wand,webp,libav,archive_cbx" -- "$args" && exit 0 || found=1
elif type gthumb > /dev/null 2>&1; then
	gthumb -- "$args" && exit 0 || found=1
elif type ristretto > /dev/null 2>&1; then
	ristretto -- "$args" && exit 0 || found=1
elif type gwenview > /dev/null 2>&1; then
	gwenview -- "$args" && exit 0 || found=1
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
	printf "clifm: No image viewer found\n" >&2
fi
exit 1
