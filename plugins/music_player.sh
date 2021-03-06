#!/bin/bash

# Music player plugin for CliFM
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if ! [[ $(type -P mplayer) ]]; then
	echo "CliFM: mplayer: Command not found" >&2
	exit $ERROR;
fi

if [[ -z "$1" ]]; then
	echo "CliFM: Missing argument" >&2
	exit $ERROR
fi

TMP_FILE="/tmp/clifm/playlist.$(tr -dc A-Za-z0-9 </dev/urandom | head -c 6)"

for file in "$@"; do
	if [[ ${file[0]} != '/' ]]; then
		echo "$PWD/$file" | sed 's/\\//g' >> "$TMP_FILE"
	else
		echo "$file" | sed 's/\\//g' >> "$TMP_FILE"
	fi
done

"${EDITOR:=nano}" "$TMP_FILE"

mplayer -playlist "$TMP_FILE"

rm -rf -- "$TMP_FILE"

exit $SUCCESS
