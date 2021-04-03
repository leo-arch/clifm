#!/bin/bash

# CliFM plugin to navigate the jump database via fzf/Rofi
# Written by L. Abramovich

if [[ $(type -P fzf) ]]; then
	finder="fzf"

elif [[ $(type -P rofi) ]]; then
	finder="rofi"

else
	echo "CliFM: No finder found. Install either fzf or rofi" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:-${HOME}/.config}/clifm/profiles/$CLIFM_PROFILE/jump.cfm"

if ! [[ -f "$FILE" ]]; then
	exit 1
fi

if [[ $finder == "fzf" ]]; then
	path="$(cut -d ":" -f3 "$FILE" | fzf --prompt="CliFM> ")"
else
	path="$(cut -d ":" -f3 "$FILE" | rofi -dmenu -p CliFM)"
fi

if [[ -n "$path" ]]; then
	echo "$path" > "$CLIFM_BUS"
fi

exit 0
