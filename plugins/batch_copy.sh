#!/bin/sh

# CliFM plugin to copy file(s) into multiple destinations
# Written by L. Abramovich
# License: GPL3

# Description: Copy files passed as arguments to files specified via
# a text editor

if [ -z "$1" ] || [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Copy one or more files/directories passed as arguments to \
one or more files/directories specified via a text editor\n"
	printf "Usage: %s FILE(s)\n" "$name"
	exit 0
fi

DEST=$(mktemp "${TMPDIR:-/tmp}/clifm_bcd.XXXXXX")

printf "# Write here destinty files/directories, one per line.\n\
# Blank and commented lines are ommited\n" > "$DEST"

"${EDITOR-:nano}" "$DEST"

for file in "$@"; do
 	grep -v "^$\|^#" "$DEST" | xargs -n1 cp -v "$file"
done

rm -f -- "$DEST"

exit 0
