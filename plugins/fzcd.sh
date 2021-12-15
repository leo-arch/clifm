#!/bin/sh

# Directory change plugin for CliFM

# Find and change directory using find and fzf
# Author: Docbroke
# License: GPL3

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "%s" "CliFM: fzf: Command not found\n" >&2
	exit 1
fi

if [ -n "$CLIFM_NO_COLOR" ] || [ -n "$NO_COLOR" ]; then
	color_opt="bw"
else
	color_opt="fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold"
fi

DIR="$(find / -type d -print0 2> /dev/null | \
fzf --read0 --prompt "Change DIR: " \
--reverse --height 15 \
--bind "tab:accept" --info=inline \
--color="$color_opt")"

if [ -n "$DIR" ]; then
    printf "%s\n" "$DIR" > "$CLIFM_BUS"
fi

exit 0
