#!/bin/sh

# Directory change plugin for CliFM

# Find and change directory using find and fzf
# Author: Docbroke
# License: GPL3

# Find the helper file
get_helper_file()
{
	helper_file="${XDG_CONFIG_HOME:-$HOME/.config}/clifm/plugins/.plugins-helper"
	if ! [ -f "$helper_file" ]; then
		helper_file="/usr/share/clifm/plugins/.plugins-helper"
		if ! [ -f "$helper_file" ]; then
			printf "CliFM: .plugins-helper: File not found\n" >&2
			exit 1
		fi
	fi
}

if ! type fzf > /dev/null >2&1; then
	printf "%s" "CliFM: fzf: Command not found\n" >&2
	exit 1
fi

get_helper_file
. "$helper_file"

DIR="$(find / -type d -print0 2> /dev/null | \
fzf --read0 --prompt "Change DIR: " \
--reverse --height 15 \
--bind "tab:accept" --info=inline \
--color="$(get_fzf_colors)")"

if [ -n "$DIR" ]; then
    printf "%s\n" "$DIR" > "$CLIFM_BUS"
fi

exit 0
