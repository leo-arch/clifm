#!/bin/sh

# CliFM plugin to find/open/cd files in CWD using fzf/Rofi
# Written by L. Abramovich

if [ "$(which fzf)" ]; then
	finder="fzf"

elif [ "$(which rofi)" ]; then
	finder="rofi"

else
	printf "CliFM: No finder found. Install either fzf or rofi\n" >&2
	exit 1
fi

if [ "$finder" = "fzf" ]; then
	FILE="$(ls -A --group-directories-first --color=always | \
			fzf --ansi --prompt "CLiFM> ")"
else
	FILE="$(ls -A | rofi -dmenu -p CLiFM)"
fi

if [ -n "$FILE" ]; then
	printf "%s\n" "$FILE" > "$CLIFM_BUS"
fi

exit 0
