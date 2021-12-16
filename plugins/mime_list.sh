#!/bin/sh

# Plugin to list files in CWD by a given mime type using FZF
# Written by L. Abramovich
# License GPL3

mime=""

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
. "$helper_file"

find . -maxdepth 1 -mindepth 1 | \
file -F'@' -N -n --mime-type -if- | \
grep "@\ .*${mime}" | cut -d"@" -f1 | cut -d"/" -f2-10 | sort | \
fzf --reverse --height=15 --exit-0 \
--info=inline --color="$(get_fzf_colors)"

exit 0
