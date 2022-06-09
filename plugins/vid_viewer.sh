#!/bin/sh

# Video thumbnails plugin for CliFM
# Written by L. Abramovich
# License: GPL3
# Dependencies: ffmpegthumbnailer and sxiv, lsix, or feh

is_vid()
{
	if file --mime-type "$1" | grep -q "video/"; then
		echo "1"
	else
		echo "0"
	fi
}

if ! type ffmpegthumbnailer >/dev/null 2>&1; then
	printf "clifm: ffmpegthumbnailer: Command not found\n" >&2
	exit 127
fi

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Display thumbnails of VIDEO(s) or of videos contained in DIR (current working directory if omitted)
Usage: %s [VIDEO... n] [DIR]\n" "$name"
	exit 0
fi

TMP_DIR=".vidthumbs.$(tr -dc A-Za-z0-9 </dev/urandom | head -c6)"

mkdir -- "$TMP_DIR" >&2

args="${*:-.}"

for arg in $args; do
	if [ -d "$arg" ]; then
		if [ "$(printf "%s" "$arg" | tail -c1)" = '/' ]; then
			arg="${arg%?}"
		fi

		for file in "$arg"/*; do
			if [ -f "$file" ] && [ "$(is_vid "$file")" = "1" ]; then
				ffmpegthumbnailer -i "$file" -o \
				"$TMP_DIR/$(basename "$file").jpg" 2>/dev/null
			fi
		done
	else
		if [ "$(is_vid "$arg")" = "1" ]; then
			ffmpegthumbnailer -i "$arg" -o \
			"$TMP_DIR/$(basename "$arg").jpg" 2>/dev/null
		fi
	fi
done

if type sxiv >/dev/null 2>&1; then
	sxiv -aqtr -- "$TMP_DIR"
elif type feh >/dev/null 2>&1; then
	feh -tZk -- "$TMP_DIR"
elif type lsix >/dev/null 2>&1; then
	lsix "$TMP_DIR"/*
else
	printf "CliFM: No thumbails viewer found\n" >&2
	rm -rf -- "$TMP_DIR" 2>/dev/null
	exit 1
fi

rm -rf -- "$TMP_DIR" 2>/dev/null

exit 0
