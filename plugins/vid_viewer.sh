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

mkdir .vidthumbs >&2

for arg in "$@"; do

	if [[ -d "$arg" ]]; then

		if [[ ${arg: -1} == '/' ]]; then
			arg="${arg%?}"
		fi

		for file in "$arg"/*; do
			if [[ -f "$file" ]]; then
				ffmpegthumbnailer -i "$file" -o .vidthumbs/$(basename "$file").jpg 2>/dev/null
			fi
		done

	else
		ffmpegthumbnailer -i "$arg" -o .vidthumbs/$(basename "$arg").jpg 2>/dev/null
	fi

done

if [[ $(type -P sxiv) ]]; then
	sxiv -aqtr -- .vidthumbs

elif [[ $(type -P feh) ]]; then
	feh -tZk -- .vidthumbs

elif [[ $(type -P lsix) ]]; then
	lsix .vidthumbs/*

else
	echo "CliFM: No thumbails viewer found" <&2
	rm -rf .vidthumbs 2>/dev/null
	exit $ERROR
fi

rm -rf .vidthumbs 2>/dev/null

exit $SUCCESS
