#!/bin/sh

# FZF selection plugin for CliFM
# Description: Select files in the current working directory using FZF

# Written by L. Abramovich

# License: GPL3

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

HELP="Usage:

Alt-h: Toggle this help screen

TAB, Alt-down: Toggle select down

Alt-up: Toggle select up

Alt-right: Select all files

Alt-left: Deselect all files

Alt-Enter: Invert selection

Enter: Confirm selection, exit, and send selected files to CliFM Selbox

Esc: Cancel and exit"

if [ "$(tput colors)" -eq 256 ]; then
	BORDERS="--border=left"
else
	BORDERS="--no-unicode"
fi

# shellcheck disable=SC2012
ls -A --group-directories-first --color=always | \
fzf --multi --marker='*' --info=inline \
	--color "prompt:6,fg+:reverse,marker:2:bold,pointer:6,header:7" \
	--bind "alt-down:toggle+down,insert:toggle+down" \
	--bind "alt-up:toggle+up" \
	--bind "alt-right:select-all,alt-left:deselect-all" \
	--bind "alt-h:toggle-preview" --preview-window=:wrap \
	--bind "alt-enter:toggle-all" --preview "printf %s \"$HELP\"" \
	--reverse "$BORDERS" --no-sort --ansi --prompt "CliFM> " > "$TMPFILE"

# shellcheck disable=SC1007
while ISF= read -r line; do
	printf "%s\n" "$PWD/$line" >> "$CLIFM_SELFILE"
done < "$TMPFILE"

if [ -z "$DISPLAY" ]; then
	clear
else
	tput rmcup
fi

rm -f -- "$TMPFILE" > /dev/null 2>&1

#clear

exit 0
