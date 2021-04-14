#!/bin/sh

# Music player plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if ! [ "$(which mplayer 2>/dev/null)" ]; then
	printf "CliFM: mplayer: Command not found\n" >&2
	exit $ERROR;
fi

if [ -z "$1" ]; then
	printf "CliFM: Missing argument\n" >&2
	exit $ERROR
fi

TMP_FILE="/tmp/clifm/playlist.$(tr -dc A-Za-z0-9 </dev/urandom | head -c 6)"

for file in "$@"; do
	if [ "$(printf "%s\n" "$file" | cut -c 1-1 )" != '/' ]; then
		printf "%s\n" "$PWD/$file" | sed 's/\\//g' >> "$TMP_FILE"
	else
		printf "%s\n" "$file" | sed 's/\\//g' >> "$TMP_FILE"
	fi
done

"${EDITOR:=nano}" "$TMP_FILE"

mplayer -playlist "$TMP_FILE"

rm -rf -- "$TMP_FILE"

exit $SUCCESS
