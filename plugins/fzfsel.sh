#!/bin/sh

# FZF selection plugin for CLiFM
# Description: Select files in the current working directory using FZF

# Written by L. Abramovich

# License: GPL2+

# Usage: select files using Space or x keys (a to select all), and
# deselect using the z key (s to deselect all). Once done, press Enter.
# At exit, selected files will be sent to CliFM Selbox

if ! [ "$(which fzf 2>/dev/null)" ]; then
	printf "CLiFM: fzf: Command not found\n" >&2
	exit 1
fi

TMPDIR="/tmp/clifm/$CLIFM_PROFILE"
TMPFILE="$TMPDIR/${CLIFM_PROFILE}.fzfsel"

! [ -d "$TMPDIR" ] && mkdir -p "$TMPDIR"

# shellcheck disable=SC2012
ls -A --group-directories-first --color=always | \
fzf --multi --marker='*' \
	--color "prompt:6,fg+:reverse" \
	--bind "space:toggle+down,x:toggle+down" \
	--bind "z:toggle+up" \
	--bind "a:select-all,s:deselect-all" \
	--layout=reverse-list --ansi --prompt "CLiFM> " > "$TMPFILE"

# shellcheck disable=SC1007
while ISF= read -r line; do
	printf "%s\n" "$PWD/$line" >> "$CLIFM_SELFILE"
done < "$TMPFILE"

rm -- "$TMPFILE" > /dev/null 2>&1

#clear

exit 0
