#!/bin/env bash

# Image thumbnails plugin for CLiFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [[ -z "$1" ]]; then
	echo "CliFM: Missing argument" >&2
	exit $ERROR
fi

if [[ -n $CLIFM_IMG_VIEWER ]]; then
	$CLIFM_IMG_VIEWER "$@"

elif [[ $(type -P sxiv) ]]; then
	if [[ -d "$1" || -h "$1" || -n "$2" ]]; then
		sxiv -aqtr -- "$@"
	else
		sxiv -aqt -- "$@"
	fi

elif [[ $(type -P feh) ]]; then
	feh -tZ -- "$@"

elif [[ $(type -P lsix) ]]; then
	if [[ -d "$1" || -h "$1" ]]; then
		lsix "$1"/*
	else
		lsix "$@"
fi

else
	echo "CliFM: No image viewer found" >&2
	exit $ERROR
fi

exit $SUCCESS
