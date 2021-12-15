#!/bin/sh

# CliFM plugin to find/open/cd files in CWD using FZF/Rofi
# Written by L. Abramovich
# License GPL3

SUCCESS=0
ERROR=1

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
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

OS="$(uname -s)"

if [ -z "$OS" ]; then
	printf "CliFM: Unable to detect operating system\n" >&2
	exit $ERROR
fi

case "$OS" in
	Linux) ls_cmd="ls -A --group-directories-first --color=always" ;;
	*) ls_cmd="ls -A" ;;
esac

if [ "$finder" = "fzf" ]; then
	COLORS="$(tput colors)"
	if [ -n "$CLIFM_NO_COLOR" ] || [ -n "$NO_COLOR" ]; then
		color_opt="bw"
	else
		color_opt="fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold"
	fi

	# shellcheck disable=SC2012
	FILE="$($ls_cmd | fzf --ansi --prompt 'CliFM> ' \
	--reverse --height "${CLIFM_FZF_HEIGHT:-80}%" \
	--bind "tab:accept" --info=inline --color="$color_opt")"
else
	# shellcheck disable=SC2012
	FILE="$(ls -A | rofi -dmenu -p CliFM)"
fi

if [ -n "$FILE" ]; then
	printf "%s\n" "$FILE" > "$CLIFM_BUS"
fi

exit $SUCCESS
