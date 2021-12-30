#!/bin/sh

# Plugin to list files in CWD by a given mime type using FZF
# Written by L. Abramovich
# License GPL3

mime=""

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

	printf "CliFM: plugins-helper: File not found. Copy this file to $HOME/.config/clifm/plugins to fix this issue\n" >&2
	exit 1
}

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "List files in CWD by mime type\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "CliFM: fzf: Command not found\n"
	exit 1
fi

printf "Enter a MIME type or part of it ('q' to quit). Ex: image\n"
while [ -z "$mime" ]; do
	printf "Mime type: "
	read -r mime
done

{ [ "$mime" = "q" ] || [ "$mime" = "quit" ]; } && exit 0

get_helper_file
# shellcheck source=/dev/null
. "$helper_file"

# shellcheck disable=SC2154
find . -maxdepth 1 -mindepth 1 | \
file -F'@' -N -n --mime-type -if- | \
grep "@\ .*${mime}" | cut -d"@" -f1 | cut -d"/" -f2-10 | sort | \
fzf --reverse --height=15 --exit-0 \
--info=inline --color="$(get_fzf_colors)"

exit 0
