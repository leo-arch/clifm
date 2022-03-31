#!/bin/sh

# Music player plugin for CliFM
# Written by L. Abramovich
# License: GPL3

SUCCESS=0
ERROR=1
PLAYER="mplayer"
#OPTS="-playlist"

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Play music files\n"
	printf "Usage: %s FILE(s)\n" "$name"
	exit $SUCCESS
fi

if ! [ "$(which mplayer 2>/dev/null)" ]; then
	printf "CliFM: mplayer: Command not found\n" >&2
	exit $ERROR;
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

"$PLAYER" "$OPS" "$TMP_FILE"

rm -rf -- "$TMP_FILE"

exit $SUCCESS
