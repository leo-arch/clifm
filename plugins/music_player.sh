#!/bin/sh

# Music player plugin for CliFM
# Written by L. Abramovich
# License: GPL3

PLAYER="mplayer"
OPTS="-playlist"

if [ -z "$1" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Create a playlist using FILE(s) and play it\n"
	printf "Usage: %s FILE(s)\n" "$name"
	exit 0
fi

if ! type "$PLAYER" >/dev/null 2>&1; then
	printf "clifm: %s: Command not found\n" "$PLAYER" >&2
	exit 127;
fi

TMP_FILE="/tmp/clifm/playlist.$(tr -dc A-Za-z0-9 </dev/urandom | head -c 6)"

for file in "$@"; do
	if [ "$(printf "%s\n" "$file" | cut -c 1-1 )" != '/' ]; then
		printf "%s\n" "$PWD/$file" | sed 's/\\//g' >> "$TMP_FILE"
	else
		printf "%s\n" "$file" | sed 's/\\//g' >> "$TMP_FILE"
	fi
done

"${EDITOR:-nano}" "$TMP_FILE"

"$PLAYER" "$OPTS" "$TMP_FILE"

rm -rf -- "$TMP_FILE"

exit 0
