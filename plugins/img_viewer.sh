#!/bin/sh

# Image thumbnails plugin for CLiFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [ -z "$1" ]; then
	printf "CliFM: Missing argument. At least one image file must be specified\n" >&2
	exit $ERROR
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
