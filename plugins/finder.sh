#!/bin/sh

# CliFM plugin to find/open/cd files in CWD using FZF/Rofi
# Written by L. Abramovich

SUCCESS=0
ERROR=1

if [ -n "$1" ] && ([ "$1" = "--help" ] || [ "$1" = "help" ]); then
	name="$(basename "$0")"
	printf "Find/open/cd files in the current directory using FZF/Rofi. Once found, press Enter to cd/open the desired file.\n"
	printf "Usage: %s\n" "$name"
	exit $SUCCESS
fi

if [ "$(which fzf)" ]; then
	finder="fzf"

elif [ "$(which rofi)" ]; then
	finder="rofi"

else
	printf "CliFM: No finder found. Install either FZF or Rofi\n" >&2
	exit $ERROR
fi

if [ "$finder" = "fzf" ]; then
	# shellcheck disable=SC2012
	FILE="$(ls -A --group-directories-first --color=always | \
			fzf --ansi --prompt "CliFM> ")"
else
	# shellcheck disable=SC2012
	FILE="$(ls -A | rofi -dmenu -p CliFM)"
fi

if [ -n "$FILE" ]; then
	printf "%s\n" "$FILE" > "$CLIFM_BUS"
fi

exit $SUCCESS
