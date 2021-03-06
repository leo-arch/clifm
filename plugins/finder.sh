#!/bin/bash

# CliFM plugin to find/open/cd files in CWD using fzf/Rofi
# Written by L. Abramovich

if [[ $(type -P fzf) ]]; then
	finder="fzf"

elif [[ $(type -P rofi) ]]; then
	finder=rofi

else
	echo "CliFM: No finder found" >&2
	exit 1
fi

if [[ $finder == "fzf" ]]; then
	FILE="$(ls -A | fzf --prompt "CLiFM> ")"
else
	FILE="$(ls -A | rofi -dmenu -p CLiFM)"
fi

if [[ -n "$FILE" ]]; then
	echo "$FILE" > "$CLIFM_BUS"
fi

exit 0
