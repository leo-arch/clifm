#!/bin/sh

# CliFM plugin to find/open/cd files using FZF/Rofi
# Written by L. Abramovich
# License GPL3

SUCCESS=0
ERROR=1

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "Find/open/cd files using FZF/Rofi. Once found, press Enter to cd/open the desired file.\n"
	printf "Usage: %s [DIR]\n" "$name"
	printf "If DIR is not specified, the current directory is used instead\n"
	exit $SUCCESS
fi

if type fzf > /dev/null 2>&1; then
	finder="fzf"
elif type rofi > /dev/null 2>&1; then
	finder="rofi"
else
	printf "clifm: No finder found. Install either FZF or Rofi\n" >&2
	exit $ERROR
fi

OS="$(uname -s)"

if [ -z "$OS" ]; then
	printf "clifm: Unable to detect operating system\n" >&2
	exit $ERROR
fi

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

DIR="."
if [ -n "$1" ]; then
	if [ -d "$1" ]; then
		DIR="$1";
	else
		printf "clifm: %s: Not a directory\n" "$1" >&2
		exit 1
	fi
fi

case "$OS" in
	Linux) ls_cmd="ls -A --group-directories-first --color=always $DIR" ;;
	*) ls_cmd="ls -A $DIR" ;;
esac

if [ "$finder" = "fzf" ]; then
	# shellcheck disable=SC2012
	# shellcheck disable=SC2154
	FILE="$($ls_cmd | fzf --ansi --prompt "$fzf_prompt" \
	--reverse --height "$fzf_height" --info=inline \
	--header "Find files in the current directory" \
	--bind "tab:accept" --info=inline --color="$(get_fzf_colors)")"
else
	# shellcheck disable=SC2012
	FILE="$(ls -A | rofi -dmenu -p CliFM)"
fi

if [ -n "$FILE" ]; then
	f="$(echo "$FILE" | sed 's/ /\\ /g')"
	printf "open %s/%s\n" "$DIR" "$f" > "$CLIFM_BUS"
fi

exit $SUCCESS
