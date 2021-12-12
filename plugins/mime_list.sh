#!/bin/sh

# Plugin to list files in CWD by a given mime type using FZF
# Written by L. Abramovich
# License GPL3

mime=""

if [ -n "$1" ] && { [ "$1" = "--help" ] || [ "$1" = "help" ]; }; then
	name="$(basename "$0")"
	printf "List files in CWD by mime type\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if [ ! "$(which fzf 2>/dev/null)" ]; then
	printf "CliFM: fzf: Command not found\n"
	exit 1
fi

printf "Enter a MIME type or part of it ('q' to quit). Ex: image\n"
while [ -z "$mime" ]; do
	printf "Mime type: "
	read -r mime
done

{ [ "$mime" = "q" ] || [ "$mime" = "quit" ] ; } && exit 0

find . -maxdepth 1 -mindepth 1 | \
file -F'@' -N -n --mime-type -if- | \
grep "@\ .*${mime}" | cut -d"@" -f1 | cut -d"/" -f2-10 | sort | \
fzf --reverse --height=15 --exit-0 \
--info=inline --color=fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold

exit 0
