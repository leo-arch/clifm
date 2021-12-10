#!/bin/sh

# Find and change directory using find and fzf
# Author: Docbroke
# License: GPL3

DIR="$(find . -type d -print0 2> /dev/null | \
fzf --read0 --prompt "Change DIR: " \
--reverse --height 15 \
--bind "tab:accept" \
--color=fg+:reverse,bg+:236,prompt:6,pointer:2,marker:2:bold,spinner:6:bold)"

if [ -n "$DIR" ]; then
    printf "%s\n" "$DIR" > "$CLIFM_BUS"
fi

exit 0
