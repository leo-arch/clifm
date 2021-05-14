#!/bin/sh

# Image thumbnails plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "help" ]; then
	name="$(basename "$0")"
	printf "Display thumbails of FILE(s) or of those in DIR\n"
	printf "Usage: %s [FILE... n] [DIR]\n" "$name"
	exit $SUCCESS
fi

if [ -n "$CLIFM_IMG_VIEWER" ]; then
	"$CLIFM_IMG_VIEWER" "$@"

elif [ "$(which sxiv 2>/dev/null)" ]; then
#	if [ -d "$1" ] || [ -h "$1" ] || [ -n "$2" ]; then
#		sxiv -aqt -- "$@"
#	else
		sxiv -aqt -- "$@"
#	fi

elif [ "$(which feh 2>/dev/null)" ]; then
	feh -tZ -- "$@"

elif [ "$(which lsix 2>/dev/null)" ]; then
	if [ -d "$1" ] || [ -h "$1" ]; then
		lsix "$1"/*
	else
		lsix "$@"
fi

else
	printf "CliFM: No image viewer found\n" >&2
	exit $ERROR
fi

exit $SUCCESS
