#!/bin/sh

# Directory change plugin for CliFM

# Find and change directory using find and fzf
# Author: Docbroke
# License: GPL3

if [ -n "$1" ] && { [ "$1" = "-h" ] || [ "$1" = "--help" ]; }; then
	name="$(basename "$0")"
	printf "Find and change directory using FZF\n"
	printf "Usage: %s\n" "$name"
	exit 0
fi

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "%s" "CliFM: fzf: Command not found\n" >&2
	exit 1
fi

DIR="$(find / -type d -print0 2> /dev/null | \
fzf --read0 --prompt "Change DIR: " \
--reverse --height 15 \
--bind "tab:accept" --info=inline \
--color=fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold)"

if [ -n "$DIR" ]; then
    printf "%s\n" "$DIR" > "$CLIFM_BUS"
fi

exit 0
