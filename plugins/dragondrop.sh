#!/bin/sh

# Drag and drop plugin for CliFM
# Written by L. Abramovich
# License: GPL3

# Depends on dragon (https://github.com/mwh/dragon). For Archers, it
# is available on the AUR as 'dragon-drag-and-drop'

# With no arguments, it opens a window to drop files; otherwise, files
# passed as arguments are send to the Dragon window to be dragged onto
# somewhere else.

# Files dropped remotely are first downloaded via cURL into the CWD and
# then send to the SelBox, whereas files dropped locally are directly
# send to the SelBox

SUCCESS=0
ERROR=1

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Drag and drop files\n"
	printf "\nUsage: %s [FILE... n]\n" "$name"
	printf "\nWith no arguments, it opens a window to drop files (using dragon); otherwise, files passed as arguments are send to the Dragon window to be dragged onto somewhere else.\n"
	exit $SUCCESS
fi


DRAGON=""

if [ "$(which dragon-drag-and-drop 2>/dev/null)" ]; then
	DRAGON="dragon-drag-and-drop"

elif [ "$(which dragon 2>/dev/null)" ]; then
	DRAGON="dragon"

else
	printf "CliFM: Neither dragon nor dragon-drag-and-drop were found. Exiting...\n" >&2
	exit $ERROR
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

	if [ "$ret" = 0 ]; then
		exit $SUCCESS
	fi

else
	if $DRAGON "$@"; then
		exit $SUCCESS
	fi
fi

exit $ERROR
