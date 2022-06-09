#!/bin/sh

# Plugin to list files in CWD by a given mime type using FZF
# Written by L. Abramovich
# License GPL3

# Dependencies: find, file, fzf

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "-h" ]; }; then
	name="${CLIFM_PLUGIN_NAME:-$(basename "$0")}"
	printf "List files in CWD by mime type\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! type fzf > /dev/null 2>&1; then
	printf "clifm: fzf: Command not found\n" >&2
	exit 127
fi

mime=""

printf "Enter a MIME type or part of it ('q' to quit). Ex: image\n"
while [ -z "$mime" ]; do
	printf "Mime type: "
	read -r mime
done

{ [ "$mime" = "q" ] || [ "$mime" = "quit" ]; } && exit 0

# Source our plugins helper
if [ -z "$CLIFM_PLUGINS_HELPER" ] || ! [ -f "$CLIFM_PLUGINS_HELPER" ]; then
	printf "clifm: Unable to find plugins-helper file\n" >&2
	exit 1
fi
# shellcheck source=/dev/null
. "$CLIFM_PLUGINS_HELPER"

# shellcheck disable=SC2154
find . -maxdepth 1 -mindepth 1 | \
file -F'@' -N -n --mime-type -if- | \
grep "@\ .*${mime}" | cut -d"@" -f1 | cut -d"/" -f2-10 | sort | \
fzf --reverse --height="$fzf_height" --exit-0 --header "Choose a file" \
--info=inline --color="$(get_fzf_colors)"

exit 0
