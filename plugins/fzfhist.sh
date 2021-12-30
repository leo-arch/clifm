#!/bin/sh

# Command history plugin via FZF for CliFM
# Written by L. Abramovich
# License GPL3

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
	printf "Navigate and execute CliFM commands history via FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "CliFM: fzf: Command not found" >&2
	exit 1
fi

FILE="${XDG_CONFIG_HOME:=$HOME/.config}/clifm/profiles/$CLIFM_PROFILE/history.cfm"

get_helper_file
# shellcheck source=/dev/null
. "$helper_file"

# shellcheck disable=SC2154
fzf --prompt="$fzf_prompt" \
--reverse --height 15 --info=inline \
--bind "tab:accept" \
--color="$(get_fzf_colors)" \
 < "$FILE" > "$CLIFM_BUS"
printf "\n"

exit 0
