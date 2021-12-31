#!/bin/sh

# CliFM plugin to find/open/cd files in CWD using FZF/Rofi
# Written by L. Abramovich
# License GPL3

SUCCESS=0
ERROR=1

# Find the helper file
get_helper_file()
{
	file1="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/plugins-helper"
	file2="/usr/share/clifm/plugins/plugins-helper"
	file3="/usr/local/share/clifm/plugins/plugins-helper"
	file4="/boot/system/non-packaged/data/clifm/plugins/plugins-helper"
	file5="/boot/system/data/clifm/plugins/plugins-helper"

	[ -f "$file1" ] && helper_file="$file1" && return

	[ -f "$file2" ] && helper_file="$file2" && return

	[ -f "$file3" ] && helper_file="$file3" && return

	[ -f "$file4" ] && helper_file="$file4" && return

	[ -f "$file5" ] && helper_file="$file5" && return

	printf "CliFM: plugins-helper: File not found. Copy this file to ~/.config/clifm/plugins to fix this issue\n" >&2
	exit 1
}

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "Find/open/cd files in the current directory using FZF/Rofi. Once found, press Enter to cd/open the desired file.\n"
	printf "Usage: %s\n" "$name"
	exit $SUCCESS
fi

if type fzf > /dev/null 2>&1; then
	finder="fzf"
elif type rofi > /dev/null 2>&1; then
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

get_helper_file
# shellcheck source=/dev/null
. "$helper_file"

case "$OS" in
	Linux) ls_cmd="ls -A --group-directories-first --color=always" ;;
	*) ls_cmd="ls -A" ;;
esac

if [ "$finder" = "fzf" ]; then
	# shellcheck disable=SC2012
	# shellcheck disable=SC2154
	FILE="$($ls_cmd | fzf --ansi --prompt "$fzf_prompt" \
	--reverse --height "$fzf_height" --info=inline \
	--header "Find files in the current directory"
	--bind "tab:accept" --info=inline --color="$(get_fzf_colors)")"
else
	# shellcheck disable=SC2012
	FILE="$(ls -A | rofi -dmenu -p CliFM)"
fi

if [ -n "$FILE" ]; then
	printf "%s\n" "$FILE" > "$CLIFM_BUS"
fi

exit $SUCCESS
