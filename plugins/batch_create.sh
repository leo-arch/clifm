#!/bin/sh

# CliFM plugin to create multiple files and/or dirs at once
# Written by L. Abramovich
# License: GPL3

# Dependencies: xargs, text editor

if [ -n "$1" ] && [ "$1" = "--help" ]; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Open a text editor to add files and/or directories to be created (file names endig with a slash will be taken as directory names). After saving the file and closing the editor, files will be created."
	printf "Usage: %s\n" "$name"
	exit 0
fi

TMP_DIR="${TMPDIR:-/tmp}"
TMP=$(mktemp "$TMP_DIR/clifm_bn.XXXXXX")
FILES=$(mktemp "$TMP_DIR/clifm_bn.XXXXXX")
DIRS=$(mktemp "$TMP_DIR/clifm_bn.XXXXXX")

dirsn=0
filesn=0

printf "# CliFM - Create files in batch \n\
# Write here file names to be created \n\
# End file name with a slash to create a directory \n\
# Empty and commented lines are omitted \n\
# Just quit the editor to cancel the operation\n\n" > "$TMP"

"${EDITOR:-nano}" "$TMP"

while read -r line; do
	if [ ${#line} = 0 ] || [ "$(printf "%.1s" "$line")" = '#' ]; then
		continue
	fi

	if [ "$(printf "%s" "$line" | tail -c1)" = "/" ]; then
		printf "%s\n" "$line" >> "$DIRS"
		dirsn=$((dirsn + 1))
	else
		printf "%s\n" "$line" >> "$FILES"
		case "$line" in
			*/*) mkdir -p "${line%/*}" ;;
		esac
		filesn=$((filesn + 1))
	fi

done < "$TMP"

if [ $dirsn -gt 0  ]; then
	xargs -I{} mkdir -p {} < "$DIRS"
fi

if [ $filesn -gt 0 ]; then
	xargs -I{} touch {} < "$FILES"
fi

rm -f -- "$TMP" "$FILES" "$DIRS" 2>/dev/null

exit 0
