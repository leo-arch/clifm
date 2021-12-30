#!/bin/sh

# Directory change plugin for CliFM

# Find and change directory using find and fzf
# Author: Docbroke
# License: GPL3

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

if ! type fzf > /dev/null 2>&1; then
	printf "%s" "CliFM: fzf: Command not found\n" >&2
	exit 1
fi

get_helper_file
# shellcheck source=/dev/null
. "$helper_file"

# shellcheck disable=SC2154
DIR="$(find / -type d -print0 2> /dev/null | \
fzf --read0 --prompt "Change DIR: " \
--reverse --height 15 \
--bind "tab:accept" --info=inline \
--color="$(get_fzf_colors)")"

if [ -n "$DIR" ]; then
    printf "%s\n" "$DIR" > "$CLIFM_BUS"
fi

exit 0
