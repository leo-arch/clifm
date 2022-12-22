#!/bin/sh

# Drag and drop plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Description:
# With no arguments, it opens a window to drop files; otherwise, files
# passed as arguments are send to the Dragon window to be dragged onto
# somewhere else.
#
# Files dropped remotely are first downloaded via cURL into the CWD and
# then send to the SelBox, whereas files dropped locally are directly
# send to the SelBox

# Dependencies:
# dragon (https://github.com/mwh/dragon), grep, sed, curl, xargs


if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Drag and drop files\n"
	printf "\n\x1b[1mUSAGE\x1b[0m\n  %s [FILE... n]\n" "$name"
	printf "\nWith no arguments, it opens a window to drop files (using dragon).
Otherwise, files passed as arguments are sent to the Dragon window
to be dragged onto somewhere else.\n"
	exit 0
fi

DRAGON=""

if type dragon-drag-and-drop >/dev/null 2>&1; then
	DRAGON="dragon-drag-and-drop"
elif type dragon >/dev/null 2>&1; then
	DRAGON="dragon"
else
	printf "clifm: Neither dragon nor dragon-drag-and-drop were found. Exiting...\n" >&2
	exit 1
fi

if [ -z "$1" ]; then
	ret=$($DRAGON --print-path --target | while read -r r; do
		if printf "%s\n" "$r" \
		| grep -q '^\(https\?\|ftps\?\|s\?ftp\):\/\/'; then
			curl -LJO "$r"
			printf "%s\n" "$PWD/$(basename "$r")" >> "$CLIFM_SELFILE"
		else
			printf "%s\n" "$r" >> "$CLIFM_SELFILE"
		fi
	done)

	[ "$ret" = 0 ] && exit 0
	exit 1
else
	echo "$@" | sed 's/\\ /\t/g;s/ /\n/g;s/\t/ /g;s/\\//g' | xargs -d'\n' "$DRAGON"
	exit "$?"
fi
