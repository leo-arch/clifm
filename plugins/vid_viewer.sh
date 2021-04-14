#!/bin/sh

# Video thumbnails plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if ! [ "$(which ffmpegthumbnailer 2>/dev/null)" ]; then
	printf "CliFM: ffmpegthumbnailer: Command not found\n" >&2
	exit $ERROR
fi

if [ -z "$1" ]; then
	printf "CliFM: Missing argument\n" >&2
	exit $ERROR
fi

TMP_DIR=".vidthumbs.$(tr -dc A-Za-z0-9 </dev/urandom | head -c6)"

mkdir -- "$TMP_DIR" >&2

for arg in "$@"; do

	if [ -d "$arg" ]; then

		if [ ${arg: -1} = '/' ]; then
			arg="${arg%?}"
		fi

		for file in "$arg"/*; do
			if [ -f "$file" ]; then
				ffmpegthumbnailer -i "$file" -o \
				"$TMP_DIR/$(basename "$file").jpg" 2>/dev/null
			fi
		done

	else
		ffmpegthumbnailer -i "$arg" -o \
		"$TMP_DIR/$(basename "$arg").jpg" 2>/dev/null
	fi

done

if [ "$(which sxiv 2>/dev/null)" ]; then
	sxiv -aqtr -- "$TMP_DIR"

elif [ "$(which feh 2>/dev/null)" ]; then
	feh -tZk -- "$TMP_DIR"

elif [ "$(which lsix)" ]; then
	lsix "$TMP_DIR"/*

else
	printf "CliFM: No thumbails viewer found\n" >&2
	rm -rf -- "$TMP_DIR" 2>/dev/null
	exit $ERROR
fi

rm -rf -- "$TMP_DIR" 2>/dev/null

exit $SUCCESS
