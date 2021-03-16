#!/bin/sh

# FZF selection plugin for CLiFM
# Description: Select files in the current working directory using FZF

# Written by L. Abramovich

# License: GPL2+

# Usage: select files using Space or x keys (a to select all), and
# deselect using the z key (s to deselect all). Once done, press Enter.
# At exit, selected files will be sent to CliFM Selbox

if ! [ $(which fzf 2>/dev/null) ]; then
	echo "CLiFM: fzf: Command not found" >&2
	exit 1
fi

TMP="/tmp/fzfsel"

ls -A | fzf -m --marker='*' --bind "space:select,space:+down,x:select,x:+down,z:deselect,z:+down,a:select-all,s:deselect-all" --layout=reverse-list --prompt "CLiFM> " > "$TMP"

clear
while ISF= read line; do
	echo "${PWD}/$line" >> "$CLIFM_SELFILE"
done < "$TMP"

exit 0
