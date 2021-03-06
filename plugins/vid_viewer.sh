#!/bin/env bash

# Video thumbnails plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if ! [[ $(type -P ffmpegthumbnailer) ]]; then
	echo "CliFM: ffmpegthumbnailer: Command not found" >&2
	exit $ERROR
fi

if [[ -z "$1" ]]; then
	echo "CliFM: Missing argument" >&2
	exit $ERROR
fi

TMP_DIR=".vidthumbs.$(tr -dc A-Za-z0-9 </dev/urandom | head -c6)"

mkdir -- "$TMP_DIR" >&2

for arg in "$@"; do

	if [[ -d "$arg" ]]; then

		if [[ ${arg: -1} == '/' ]]; then
			arg="${arg%?}"
		fi

		for file in "$arg"/*; do
			if [[ -f "$file" ]]; then
				ffmpegthumbnailer -i "$file" -o \
				"$TMP_DIR/$(basename "$file").jpg" 2>/dev/null
			fi
		done

	else
		ffmpegthumbnailer -i "$arg" -o \
		"$TMP_DIR/$(basename "$arg").jpg" 2>/dev/null
	fi

done

if [[ $(type -P sxiv) ]]; then
	sxiv -aqtr -- "$TMP_DIR"

elif [[ $(type -P feh) ]]; then
	feh -tZk -- "$TMP_DIR"

elif [[ $(type -P lsix) ]]; then
	lsix "$TMP_DIR"/*

else
	echo "CliFM: No thumbails viewer found" >&2
	rm -rf -- "$TMP_DIR" 2>/dev/null
	exit $ERROR
fi

rm -rf -- "$TMP_DIR" 2>/dev/null

exit $SUCCESS
