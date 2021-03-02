#!/bin/bash

# Drag and drop plugin for CLiFM
# Written by L. Abramovich

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

DRAGON=""

if [[ $(type -P dragon-drag-and-drop) ]]; then
	DRAGON="dragon-drag-and-drop"

elif [[ $(type -P dragon) ]]; then
	DRAGON="dragon"

else
	echo "CLiFM: Neither dragon nor dragon-drag-and-drop were found. Exiting... " >&2
	exit $ERROR
fi

if [[ -z $1 ]]; then

	$DRAGON --print-path --target | while read -r r; do

		if [[ $(echo "$r" \
		| grep '^\(https\?\|ftps\?\|s\?ftp\):\/\/') ]]; then
			curl -LJO "$r"
			echo "$PWD/$(basename "$r")" >> "$CLIFM_SELFILE"

		else
			echo "$r" >> "$CLIFM_SELFILE"
		fi

	done

else
	$DRAGON "$@"
fi

if [[ $? -eq 0 ]]; then
	exit $SUCCESS
fi

exit $ERROR
